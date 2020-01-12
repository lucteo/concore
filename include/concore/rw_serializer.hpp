#pragma once

#include "task.hpp"
#include "executor_type.hpp"
#include "data/concurrent_queue.hpp"
#include "detail/utils.hpp"

#include <deque>
#include <mutex>
#include <memory>
#include <cassert>

namespace concore {

namespace detail {
struct rw_serializer_impl : std::enable_shared_from_this<rw_serializer_impl> {
    //! The base executor used to actually execute the tasks, once we've serialized them
    executor_t base_executor_;
    //! The function called to handle exceptions
    std::function<void(std::exception_ptr)> except_fun_;
    //! The queue of READ tasks
    concurrent_queue<task, queue_type::multi_prod_multi_cons> read_tasks_;
    //! The queue of WRITE tasks
    concurrent_queue<task, queue_type::multi_prod_single_cons> write_tasks_;
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

    rw_serializer_impl(executor_t base_executor, std::function<void(std::exception_ptr)> except_fun)
        : base_executor_(base_executor)
        , except_fun_(except_fun) {}

    //! Adds a new READ task to this serializer
    void enqueue_read(task&& t) {
        read_tasks_.push(std::forward<task>(t));

        // Increase the number of READ tasks.
        // If we WRITE writes, count towards the queued READs, otherwise towards the active READs.
        count_bits old, desired;
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
            enqueue_next_read();
    }
    //! Adds a new WRITE task to this serializer
    void enqueue_write(task&& t) {
        write_tasks_.push(std::forward<task>(t));

        // Increase the number of WRITE tasks
        count_bits old, desired;
        old.int_value = combined_count_.load();
        do {
            desired.int_value = old.int_value;
            desired.fields.num_writes++;
        } while (!combined_count_.compare_exchange_weak(old.int_value, desired.int_value));

        // Start the task if we weren't doing anything
        if (old.fields.num_writes == 0 && old.fields.num_active_reads == 0)
            enqueue_next_write();
    }

    //! Called by the base executor to execute READ task.
    void execute_read() {
        detail::pop_and_execute(read_tasks_, except_fun_);

        // Decrement num_active_reads
        count_bits old, desired;
        old.int_value = combined_count_.load();
        do {
            desired.int_value = old.int_value;
            desired.fields.num_active_reads--;
        } while (!combined_count_.compare_exchange_weak(old.int_value, desired.int_value));

        // If there are no more ongoing READs, but we have pending WRITEs, start the WRITEs
        // Note: READ tasks will never trigger other READ tasks; in the absence of WRITEs, the
        // enqueueing of READs is done by `enqueue_read()`.
        if (old.fields.num_active_reads == 1 && old.fields.num_writes > 0)
            enqueue_next_write();
    }
    //! Called by the base executor to execute WRITE task.
    void execute_write() {
        detail::pop_and_execute(write_tasks_, except_fun_);

        // Decrement num_writes
        // If num_writes == 0, transform pending READs into active READs
        count_bits old, desired;
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
            enqueue_next_write();
        // If we transformed pending READs into actual READs, enqueue them
        else if (old.fields.num_active_reads == 0 && old.fields.num_queued_reads > 0) {
            for (unsigned i = 0; i < old.fields.num_queued_reads; i++)
                enqueue_next_read();
        }
    }

    //! Enqueue the next READ task to be executed in the base executor.
    void enqueue_next_read() {
        base_executor_([p_this = shared_from_this()]() { p_this->execute_read(); });
    }
    //! Enqueue the next WRITE task to be executed in the base executor.
    void enqueue_next_write() {
        base_executor_([p_this = shared_from_this()]() { p_this->execute_write(); });
    }
};

//! The type of the executor used for READ tasks.
class reader_type {
    std::shared_ptr<detail::rw_serializer_impl> impl_;

public:
    reader_type(std::shared_ptr<detail::rw_serializer_impl> impl)
        : impl_(impl) {}

    void operator()(task t) { impl_->enqueue_read(std::move(t)); }
};

//! The type of the executor used for WRITE tasks.
class writer_type {
    std::shared_ptr<detail::rw_serializer_impl> impl_;

public:
    writer_type(std::shared_ptr<detail::rw_serializer_impl> impl)
        : impl_(impl) {}

    void operator()(task t) { impl_->enqueue_write(std::move(t)); }
};

} // namespace detail

inline namespace v1 {

//! Similar to a serializer but allows two types of tasks: READ and WRITE tasks.
//!
//! The READ tasks can be run in parallel with other READ tasks, but not with WRITE tasks.
//! The WRITE tasks cannot be run in parallel nether with READ nor with other WRITE tasks.
//!
//! Unlike `serializer` and `n_serializer` this is not an executor. Instead it provides two methods:
//! `reader()` and `writer()` that will return task executors. The two executors returned by these
//! two methods are, of course, related by the READ/WRITE incompatibility.
//!
//! This implementation slightly favors the WRITEs: if there are multiple pending WRITEs and
//! multiple pending READs, this will execute all the WRITEs before executing the READs. The
//! rationale is twofold:
//!     - it's expected that the number of WRITEs is somehow smaller than the number of READs
//!     (otherwise a simple serializer would probably work too)
//!     - it's expected that the READs would want to "read" the latest data published by the WRITEs
class rw_serializer {
public:
    using reader_type = detail::reader_type;
    using writer_type = detail::writer_type;

    rw_serializer(executor_t base_executor, std::function<void(std::exception_ptr)> except_fun = {})
        : impl_(std::make_shared<detail::rw_serializer_impl>(base_executor, except_fun)) {}

    //! Returns an executor that will apply treat all the given tasks as READ tasks.
    reader_type reader() const { return reader_type(impl_); }
    //! Returns an executor that will apply treat all the given tasks as WRITE tasks.
    writer_type writer() const { return writer_type(impl_); }

private:
    //! Implementation detail shared by both reader and writer types
    std::shared_ptr<detail::rw_serializer_impl> impl_;
};

} // namespace v1
} // namespace concore