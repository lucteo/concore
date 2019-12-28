#pragma once

#include "task.hpp"
#include "executor_type.hpp"
#include "detail/concurrent_queue.hpp"
#include "detail/utils.hpp"

#include <memory>
#include <atomic>
#include <cassert>

namespace concore {

inline namespace v1 {

namespace detail {

//! The implementation details of a serializer
struct serializer_impl : std::enable_shared_from_this<serializer_impl> {
    //! The base executor used to actually execute the tasks, once we've serialized them
    executor_t base_executor_;
    //! The function called to handle exceptions
    std::function<void(std::exception_ptr)> except_fun_;
    //! The queue of tasks that wait to be executed
    concurrent_queue<task> waiting_tasks_;
    //! The number of tasks that are in the queue
    std::atomic<int> count_{0};

    serializer_impl(
            executor_t base_executor, std::function<void(std::exception_ptr)> except_fun = {})
        : base_executor_(base_executor)
        , except_fun_(except_fun) {}

    //! Adds a new task to this serializer
    void enqueue(task&& t) {
        // Add the task to the queue
        waiting_tasks_.push(t);

        // If there were no other tasks, enqueue a task in the base executor
        if (count_++ == 0)
            enqueue_next();
    }

    //! Called by the base executor to execute one task.
    void execute_one() {
        detail_shared::pop_and_execute(waiting_tasks_, except_fun_);

        // If there are still tasks in the queue, continue to enqueue the next task
        if (count_-- > 1)
            enqueue_next();
    }

    //! Enqueue the next task to be executed in the base executor.
    void enqueue_next() {
        // We always wrap our tasks into `execute_one`. This way, we can handle continuations.
        base_executor_([p_this = shared_from_this()]() { p_this->execute_one(); });
    }
};
} // namespace detail

//! Executor type that allows only one task to be executed at a given time.
//!
//! This will be constructed on top of an existing executor. Given N tasks, this will "serially"
//! enqueue tasks to the base executor. When a task is done executing, the next task will be
//! enqueued to the base executor, and so on.
//!
//! Guarantees:
//! - no more than 1 task is executed at once
//! - the tasks are executed in the order they are enqueued
class serializer {
public:
    serializer(executor_t base_executor, std::function<void(std::exception_ptr)> except_fun = {})
        : impl_(std::make_shared<detail::serializer_impl>(base_executor, except_fun)) {}

    //! The call operator that makes this an executor.
    //! Enqueues the given task in the base serializer, making sure that two tasks that pass through
    //! here are not executed at the same time.
    void operator()(task t) { impl_->enqueue(std::move(t)); }

private:
    //! The implementation object of this serializer.
    //! We need this to be shared pointer for lifetime issue, but also to be able to copy the
    //! serializer easily.
    std::shared_ptr<detail::serializer_impl> impl_;
};

} // namespace v1
} // namespace concore