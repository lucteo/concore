#pragma once

#include "task_group.hpp"

#include <functional>

namespace concore {

namespace detail {
    class task_system;
}

inline namespace v1 {

using task_function = std::function<void()>;

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
    task(T&& ftor, task_group grp)
        : fun_(std::forward<T>(ftor))
        , task_group_(grp) {}
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
        std::swap(task_group_, other.task_group_);
    }

    explicit operator bool() const noexcept { return static_cast<bool>(fun_); }

    //! The call operator; called to execute the task
    void operator()() { fun_(); }

    //! Return the task_group object overseeing this task
    task_group& get_task_group() { return task_group_; }

private:
    //! The function to be called
    std::function<void()> fun_;
    //! The group that this tasks belongs to
    task_group task_group_;

    // give task_system access to task_group_
    friend detail::task_system;
};
} // namespace v1

} // namespace concore