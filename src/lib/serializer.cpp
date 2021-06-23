#include "concore/serializer.hpp"
#include "concore/global_executor.hpp"
#include "concore/spawn.hpp"
#include "concore/task_cancelled.hpp"
#include "concore/data/concurrent_queue.hpp"
#include "concore/detail/utils.hpp"
#include "concore/detail/enqueue_next.hpp"

#include <atomic>
#include <cassert>

namespace concore {

inline namespace v1 {

//! The implementation details of a serializer
struct serializer::impl : std::enable_shared_from_this<impl> {
    //! The base executor used to actually execute the tasks, when we enqueue them
    any_executor base_executor_;
    //! The executor to be used when
    any_executor cont_executor_;
    //! Handler to be called whenever we have an exception while enqueueing the next task
    except_fun_t except_fun_;
    //! The queue of tasks that wait to be executed
    concurrent_queue<task> waiting_tasks_;
    //! The number of tasks that are in the queue
    std::atomic<int> count_{0};

    impl(any_executor base_executor, any_executor cont_executor)
        : base_executor_(base_executor)
        , cont_executor_(cont_executor) {
        if (!base_executor)
            base_executor_ = global_executor{};
        if (!cont_executor)
            cont_executor_ = base_executor ? base_executor : spawn_continuation_executor{};
    }

    //! Adds a new task to this serializer
    void enqueue(task&& t) {
        // Add the task to the queue, with the right continuation
        set_continuation(t);
        waiting_tasks_.push(std::forward<task>(t));

        // If there were no other tasks, enqueue a task in the base executor
        if (count_++ == 0)
            start_next_task(base_executor_);
    }

    //! Called when the continuation of the wrapper task is executed to move to the next task
    void on_cont(std::exception_ptr) {
        // task exceptions are not reported through except_fun_
        if (count_-- > 1)
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
        auto t = detail::pop_task(waiting_tasks_);
        detail::enqueue_next(exec, std::move(t), except_fun_);
    }
};

serializer::serializer(any_executor base_executor, any_executor cont_executor)
    : impl_(std::make_shared<impl>(base_executor, cont_executor)) {}

void serializer::do_enqueue(task t) const { impl_->enqueue(std::move(t)); }

void serializer::set_exception_handler(except_fun_t except_fun) {
    impl_->except_fun_ = std::move(except_fun);
}

} // namespace v1
} // namespace concore