#pragma once

#include "task_control.hpp"

#include <functional>

namespace concore {

inline namespace v1 {

//! A task. In essence, just a function that takes no arguments and returns nothing.
//! This can be enqueued into an executor and executed at a later time, and/or with certain
//! constraints.
class task {
public:
    task() = default;

    template <typename T>
    task(T&& ftor)
        : fun_(std::forward<T>(ftor)) {}
    template <typename T>
    task(T&& ftor, task_control tc)
        : fun_(std::forward<T>(ftor))
        , task_control_(tc) {}
    template <typename T>
    task& operator=(T&& ftor) {
        fun_ = std::forward<T>(ftor);
        return *this;
    }

    task(task&&) = default;
    task& operator=(task&&) = default;

    task(const task&) = delete;
    task& operator=(const task&) = delete;

    void swap(task& other) {
        fun_.swap(other.fun_);
        std::swap(task_control_, other.task_control_);
    }

    explicit operator bool() const noexcept { return static_cast<bool>(fun_); }

    //! The call operator; called to execute the task
    void operator()() { fun_(); }

    //! Return the task_control object overseeing this task
    task_control& get_task_control() { return task_control_; }

private:
    //! The function to be called
    std::function<void()> fun_;
    //! The object used to control cancellation of this task
    task_control task_control_;
};
} // namespace v1

} // namespace concore