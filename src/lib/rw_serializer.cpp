#include "concore/rw_serializer.hpp"
#include "concore/global_executor.hpp"
#include "concore/spawn.hpp"
#include "concore/data/concurrent_queue.hpp"
#include "concore/detail/utils.hpp"
#include "concore/detail/enqueue_next.hpp"

#include <deque>
#include <atomic>
#include <cassert>

namespace concore {

inline namespace v1 {

//! The implementation details of a rw_serializer
struct rw_serializer::impl : std::enable_shared_from_this<impl> {
    //! The base executor used to actually execute the tasks, when we enqueue them
    any_executor base_executor_;
    //! The executor to be used when
    any_executor cont_executor_;
    //! Handler to be called whenever we have an exception while enqueueing the next task
    except_fun_t except_fun_;
    //! The queue of READ tasks
    concurrent_queue<task> read_tasks_;
    //! The queue of WRITE tasks
    concurrent_queue<task> write_tasks_;
    //! The number of READ and WRITE tasks in the queues; interpreted with `count_bits`
    std::atomic<uint64_t> combined_count_{0};

    union count_bits {
        uint64_t int_value;
        struct {
            uint32_t num_writes : 16;       //!< Number of write tasks added
            uint32_t num_active_reads : 16; //!< Number of READ tasks added in the absence WRITEs
            uint32_t num_queued_reads : 32; //!< Number of READ tasks added after a WRITE
        } fields;
    };

    impl(any_executor base_executor, any_executor cont_executor)
        : base_executor_(base_executor)
        , cont_executor_(cont_executor) {
        if (!base_executor)
            base_executor_ = global_executor{};
        if (!cont_executor)
            cont_executor_ = base_executor ? base_executor : spawn_continuation_executor{};
    }

    //! Adds a new READ task to this serializer
    void enqueue_read(task&& t) {
        // Add the task to the READ queue, with the right continuation
        set_continuation_read(t);
        read_tasks_.push(std::forward<task>(t));

        // Increase the number of READ tasks.
        // If we WRITE writes, count towards the queued READs, otherwise towards the active READs.
        count_bits old{}, desired{};
        old.int_value = combined_count_.load();
        do {
            desired.int_value = old.int_value;
            if (old.fields.num_writes > 0)
                desired.fields.num_queued_reads++;
            else
                desired.fields.num_active_reads++;
        } while (!combined_count_.compare_exchange_weak(old.int_value, desired.int_value));

        // Start executing the task only if we don't have any WRITE task (this is an active READ)
        if (old.fields.num_writes == 0)
            detail::enqueue_next(base_executor_, detail::pop_task(read_tasks_), except_fun_);
    }
    //! Adds a new WRITE task to this serializer
    void enqueue_write(task&& t) {
        // Add the task to the WRITE queue, with the right continuation
        set_continuation_write(t);
        write_tasks_.push(std::forward<task>(t));

        // Increase the number of WRITE tasks
        count_bits old{}, desired{};
        old.int_value = combined_count_.load();
        do {
            desired.int_value = old.int_value;
            desired.fields.num_writes++;
        } while (!combined_count_.compare_exchange_weak(old.int_value, desired.int_value));

        // Start the task if we weren't doing anything
        if (old.fields.num_writes == 0 && old.fields.num_active_reads == 0)
            detail::enqueue_next(base_executor_, detail::pop_task(write_tasks_), except_fun_);
    }

    //! Called when a READ task is done to move to the next task
    void on_cont_read(std::exception_ptr) {
        // Decrement num_active_reads
        count_bits old{}, desired{};
        old.int_value = combined_count_.load();
        do {
            desired.int_value = old.int_value;
            desired.fields.num_active_reads--;
        } while (!combined_count_.compare_exchange_weak(old.int_value, desired.int_value));

        // If there are no more ongoing READs, but we have pending WRITEs, start the WRITEs
        // Note: READ tasks will never trigger other READ tasks; in the absence of WRITEs, the
        // enqueueing of READs is done by `enqueue_read()`.
        if (old.fields.num_active_reads == 1 && old.fields.num_writes > 0)
            detail::enqueue_next(cont_executor_, detail::pop_task(write_tasks_), except_fun_);
    }
    //! Called when a WRITE task is done to move to the next task
    void on_cont_write(std::exception_ptr) {
        // Decrement num_writes
        // If num_writes == 0, transform pending READs into active READs
        count_bits old{}, desired{};
        old.int_value = combined_count_.load();
        assert(old.fields.num_active_reads == 0);
        do {
            desired.int_value = old.int_value;
            if (desired.fields.num_writes-- == 1) {
                desired.fields.num_active_reads = desired.fields.num_queued_reads;
                desired.fields.num_queued_reads = 0;
            }
        } while (!combined_count_.compare_exchange_weak(old.int_value, desired.int_value));

        // If we have more WRITEs, enqueue them
        if (desired.fields.num_writes > 0)
            detail::enqueue_next(cont_executor_, detail::pop_task(write_tasks_), except_fun_);
        // If we transformed pending READs into actual READs, enqueue them
        else if (old.fields.num_active_reads == 0 && old.fields.num_queued_reads > 0) {
            for (unsigned i = 0; i < old.fields.num_queued_reads; i++)
                detail::enqueue_next(cont_executor_, detail::pop_task(read_tasks_), except_fun_);
        }
    }

    //! Set the continuation of the READ task, so that the serializer works.
    //! If the task already has a continuation, that would be called first.
    void set_continuation_read(task& t) {
        auto inner_cont = t.get_continuation();
        task_continuation_function cont;
        if (inner_cont) {
            cont = [inner_cont, p_this = shared_from_this()](std::exception_ptr ex) {
                inner_cont(ex);
                p_this->on_cont_read(std::move(ex));
            };
        } else {
            cont = [p_this = shared_from_this()](
                           std::exception_ptr ex) { p_this->on_cont_read(std::move(ex)); };
        }
        t.set_continuation(std::move(cont));
    }
    //! Set the continuation of the WRITE task, so that the serializer works.
    //! If the task already has a continuation, that would be called first.
    void set_continuation_write(task& t) {
        auto inner_cont = t.get_continuation();
        task_continuation_function cont;
        if (inner_cont) {
            cont = [inner_cont, p_this = shared_from_this()](std::exception_ptr ex) {
                inner_cont(ex);
                p_this->on_cont_write(std::move(ex));
            };
        } else {
            cont = [p_this = shared_from_this()](
                           std::exception_ptr ex) { p_this->on_cont_write(std::move(ex)); };
        }
        t.set_continuation(std::move(cont));
    }
};

rw_serializer::reader_type::reader_type(std::shared_ptr<impl> impl)
    : impl_(std::move(impl)) {}

void rw_serializer::reader_type::do_enqueue(task t) const { impl_->enqueue_read(std::move(t)); }
void rw_serializer::reader_type::do_enqueue_noexcept(task t) const noexcept {
    try {
        impl_->enqueue_read(std::move(t));
    } catch (...) {
        auto cont = t.get_continuation();
        if (cont)
            cont(std::current_exception());
    }
}

rw_serializer::writer_type::writer_type(std::shared_ptr<impl> impl)
    : impl_(std::move(impl)) {}

void rw_serializer::writer_type::do_enqueue(task t) const { impl_->enqueue_write(std::move(t)); }
void rw_serializer::writer_type::do_enqueue_noexcept(task t) const noexcept {
    try {
        impl_->enqueue_write(std::move(t));
    } catch (...) {
        auto cont = t.get_continuation();
        if (cont)
            cont(std::current_exception());
    }
}

rw_serializer::rw_serializer(any_executor base_executor, any_executor cont_executor)
    : impl_(std::make_shared<impl>(base_executor, cont_executor)) {}

void rw_serializer::set_exception_handler(except_fun_t except_fun) {
    impl_->except_fun_ = std::move(except_fun);
}

} // namespace v1
} // namespace concore