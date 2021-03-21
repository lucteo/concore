#include "concore/n_serializer.hpp"
#include "concore/global_executor.hpp"
#include "concore/spawn.hpp"
#include "concore/detail/consumer_bounded_queue.hpp"
#include "concore/detail/utils.hpp"

#include <atomic>
#include <cassert>

namespace concore {

inline namespace v1 {

struct n_serializer::impl : std::enable_shared_from_this<impl> {
    //! The base executor used to actually execute the tasks, when we enqueue them
    any_executor base_executor_;
    //! The executor to be used when
    any_executor cont_executor_;
    //! Handler to be called whenever we have an exception while enqueueing the next task
    except_fun_t except_fun_;
    //! The tasks that are enqueued into our object
    detail::consumer_bounded_queue<task> processing_items_;

    impl(int N, any_executor base_executor, any_executor cont_executor)
        : base_executor_(base_executor)
        , cont_executor_(cont_executor)
        , processing_items_(N) {
        if (!base_executor)
            base_executor_ = global_executor{};
        if (!cont_executor)
            cont_executor_ = base_executor ? base_executor : spawn_continuation_executor{};
    }

    void enqueue(task&& t) {
        // Add the task to the queue, with the right continuation
        set_continuation(t);
        if (processing_items_.push_and_try_acquire(std::move(t)))
            start_next_task(base_executor_);
    }

    //! Called when the continuation of the wrapper task is executed to move to the next task
    void on_cont(std::exception_ptr) {
        if (processing_items_.release_and_acquire())
            start_next_task(cont_executor_);
    }

    //! Set the continuation of the task, so that the serializer works.
    //! If the task already has a continuation, that would be called first.
    void set_continuation(task& t) {
        auto inner_cont = t.get_continuation();
        task_continuation_function cont;
        if (inner_cont) {
            cont = [inner_cont, p_this = shared_from_this()](std::exception_ptr ex) {
                inner_cont(ex);
                p_this->on_cont(std::move(ex));
            };
        } else {
            cont = [p_this = shared_from_this()](
                           std::exception_ptr ex) { p_this->on_cont(std::move(ex)); };
        }
        t.set_continuation(std::move(cont));
    }
    //! Start executing the next task in our serializer
    void start_next_task(const any_executor& exec) {
        auto t = processing_items_.extract_one();
        try {
            exec.execute(std::move(t));
        } catch (...) {
            // Somehow the executor can throw, but after executing the task -- weird, right?
            auto ex = std::current_exception();
            except_fun_(ex);
        }
    }
};

n_serializer::n_serializer(int N, any_executor base_executor, any_executor cont_executor)
    : impl_(std::make_shared<impl>(N, base_executor, cont_executor)) {}

void n_serializer::do_enqueue(task t) const { impl_->enqueue(std::move(t)); }

void n_serializer::set_exception_handler(except_fun_t except_fun) {
    impl_->except_fun_ = std::move(except_fun);
}

} // namespace v1
} // namespace concore