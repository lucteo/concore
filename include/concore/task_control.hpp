#pragma once

// #include "detail/task_system.hpp"

#include <functional>
#include <exception>
#include <memory>

namespace concore {

inline namespace v1 {
class task;
class task_control;
} // namespace v1

namespace detail {
struct task_control_impl;

//! Structure used by the task system to interact with task_control objects
struct task_control_access {
    //! Called just before executing a task, to let the task_control know
    static void on_starting_task(task_control& tc, const task& t);
    //! Called just after a task is completed without an exception
    static void on_task_done(task_control& tc, const task& t);
    //! Called when the task throws an exception
    static void on_task_exception(task_control& tc, const task& t, std::exception_ptr ex);
};

} // namespace detail

inline namespace v1 {

// Forward declaration
class task;

//! Class used to control the lifetime of a task: cancellation and waiting for the task.
//!
//! Tasks can point to one task_control object. A task_control object can point to a parent
//! task_control object, thus creating hierarchies of task_control objects.
//!
//! Scenario 1: cancellation
//!     - user can tell a task_control object to cancel the tasks
//!     - any tasks that use this task_control object will not be executed anymore
//!     - in-progress tasks can check from time to time whether the task is canceled
//!
//! Scenario 2: waiting for tasks
//!     - a dedicated task_control can be created for a task that needs waiting
//!     - only that task will point to this task_control
//!     - whenever the task is finished it will notify the task_control object
//!     - user can query whether the task is ready or not
class task_control {
public:
    //! Tag to distinguish ctor with parent from a copy ctor
    struct with_parent {};

    //! Creates an empty task_control; no operations can be called on it
    task_control();
    ~task_control();

    task_control(const task_control&) = default;
    task_control(task_control&&) = default;
    task_control& operator=(const task_control&) = default;
    task_control& operator=(task_control&&) = default;

    //! Creates a task_control object, with an optional parent
    static task_control create(const task_control& parent = {});

    //! Checks if this is a valid task control object
    explicit operator bool() const { return static_cast<bool>(impl_); }

    //! Set the function to be called whenever an exception is thrown by a task.
    void set_exception_handler(std::function<void(std::exception_ptr)> except_fun);

    //! Cancels the execution of the current tasks, and any tasks that will be enqueued from now on.
    void cancel();

    //! Clears the cancel flag; new tasks can be executed again.
    void clear_cancel();

    //! Checks if the tasks overseen by this object are canceled
    bool is_cancelled() const;

    //! Returns the task_control object for the current executing task.
    //! This uses TLS to get the task_control from the current thread.
    //! Returns an empty task_control if no task is running (but then, are you calling this from
    //! within a task?)
    static task_control current_task_control();

    //! To be called from within the execution of a task to check if the current task should be
    //! canceled or not.
    //! Returns false if this is called outside of an executing task.
    static bool is_current_task_cancelled();

    //! Checks whether there are tasks or other task_control objects associated with this object
    bool is_active() const;

private:
    //! Implementation detail of a task control object
    std::shared_ptr<detail::task_control_impl> impl_;

    friend detail::task_control_access;
};

} // namespace v1

} // namespace concore