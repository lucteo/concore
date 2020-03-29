#pragma once

#include "task.hpp"
#include "executor_type.hpp"

#include <memory>

namespace concore {

inline namespace v1 {

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
    explicit serializer(executor_t base_executor = {}, executor_t cont_executor = {});

    //! The call operator that makes this an executor.
    //! Enqueues the given task in the base serializer, making sure that two tasks that pass through
    //! here are not executed at the same time.
    void operator()(task t);

private:
    struct impl;

    //! The implementation object of this serializer.
    //! We need this to be shared pointer for lifetime issue, but also to be able to copy the
    //! serializer easily.
    std::shared_ptr<impl> impl_;
};

} // namespace v1
} // namespace concore