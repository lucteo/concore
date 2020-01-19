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

//! An executor that executes all the tasks immediately, when called. No delayed execution.
constexpr auto immediate_executor = detail::immediate_executor_type{};

} // namespace concore