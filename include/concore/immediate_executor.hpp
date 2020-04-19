#pragma once

#include "task.hpp"

namespace concore {

namespace detail {

//! The type of the immediate executor. When called with a task, executes the task immediately.
//! If the given task is a lengthy one, calling this executor will block the current thread until
//! the task finishes executing.
struct immediate_executor_type {
    //! The call operator required by executor. Takes any type of task (with no params) to execute.
    template <typename F>
    void operator()(F&& f) const {
        std::forward<F>(f)();
    }
};

} // namespace detail

inline namespace v1 {

/**
 * @brief      Executor that executes all the tasks immediately.
 *
 * Most executors are storing the tasks for later execution and the enqueueing finishes very fast.
 * This one executes the task during enqueueing, without scheduling it for a later time.
 */
constexpr auto immediate_executor = detail::immediate_executor_type{};

} // namespace v1
} // namespace concore