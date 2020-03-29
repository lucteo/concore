#pragma once

#include <functional>
#include <exception>
#include <memory>

namespace concore {

inline namespace v1 {
class task;
class task_group;
} // namespace v1

namespace detail {
struct task_group_impl;

//! Structure used by the task system to interact with task_group objects
struct task_group_access {
    //! Called just before executing a task, to let the task_group know
    static void on_starting_task(task_group& grp, const task& t);
    //! Called just after a task is completed without an exception
    static void on_task_done(task_group& grp, const task& t);
    //! Called when the task throws an exception
    static void on_task_exception(task_group& grp, const task& t, std::exception_ptr ex);
};

} // namespace detail

inline namespace v1 {

// Forward declaration
class task;

//! Class used to control the lifetime of tasks: cancellation and waiting for task completion.
//!
//! Tasks can point to one task_group object. A task_group object can point to a parent
//! task_group object, thus creating hierarchies of task_group objects.
//!
//! Scenario 1: cancellation
//!     - user can tell a task_group object to cancel the tasks
//!     - any tasks that use this task_group object will not be executed anymore
//!     - in-progress tasks can check from time to time whether the task is canceled
//!
//! Scenario 2: waiting for tasks
//!     - a dedicated task_group can be created for a task that needs waiting
//!     - only that task will point to this task_group
//!     - whenever the task is finished it will notify the task_group object
//!     - user can query whether all the tasks in a group are finished
//!
//! Besides this, a task group also defines the handler to be called when a task belonging to the
//! group throws an exception.
class task_group {
public:
    //! Creates an empty task_group; no operations can be called on it
    task_group();
    ~task_group();

    task_group(const task_group&) = default;
    task_group(task_group&&) = default;
    task_group& operator=(const task_group&) = default;
    task_group& operator=(task_group&&) = default;

    //! Creates a task_group object, with an optional parent
    static task_group create(const task_group& parent = {});

    //! Checks if this is a valid task group object
    explicit operator bool() const { return static_cast<bool>(impl_); }

    //! Set the function to be called whenever an exception is thrown by a task.
    void set_exception_handler(std::function<void(std::exception_ptr)> except_fun);

    //! Cancels the execution of the current tasks, and any tasks that will be enqueued from now on.
    void cancel();

    //! Clears the cancel flag; new tasks can be executed again.
    void clear_cancel();

    //! Checks if the tasks overseen by this object are canceled
    bool is_cancelled() const;

    //! Checks whether there are tasks or other task_group objects associated with this object
    bool is_active() const;

    //! Returns the task_group object for the current executing task.
    //! This uses TLS to get the task_group from the current thread.
    //! Returns an empty task_group if no task is running (but then, are you calling this from
    //! within a task?)
    static task_group current_task_group();

    //! To be called from within the execution of a task to check if the current task should be
    //! canceled or not.
    //! Returns false if this is called outside of an executing task.
    static bool is_current_task_cancelled();

private:
    //! Implementation detail of a task group object
    std::shared_ptr<detail::task_group_impl> impl_;

    friend detail::task_group_access;
};

} // namespace v1

} // namespace concore