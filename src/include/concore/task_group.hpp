/**
 * @file    task_group.hpp
 * @brief   Defines the @ref concore::v1::task_group "task_group" class
 *
 * @see     @ref concore::v1::task_group "task_group"
 */
#pragma once

#include "except_fun_type.hpp"

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
    static void on_starting_task(const task_group& grp);
    //! Called just after a task is completed without an exception
    static void on_task_done(const task_group& grp);
    //! Called when the task throws an exception
    static void on_task_exception(const task_group& grp, std::exception_ptr ex);

    //! Called when a task was created in a group; the task starts to be "active"
    static void on_task_created(const task_group& grp);
    //! Called when a task from the group is destroyed; the task is executed, and it's not "active"
    //! anymore.
    static void on_task_destroyed(const task_group& grp);
};

} // namespace detail

inline namespace v1 {

// Forward declaration
class task;

/**
 * @brief      Used to control a group of tasks (cancellation, waiting, exceptions).
 *
 * Tasks can point to one task_group object. A task_group object can point to a parent
 * task_group object, thus creating hierarchies of task_group objects.
 *
 * task_group implements shared-copy semantics. If one makes a copy of a task_group object, the
 * actual value of the task_group will be shared. For example, if we cancel one task_group, the
 * second task_group is also canceled. concore takes advantage of this type of semantics and takes
 * all task_group objects by copy, while the content is shared.
 *
 * **Scenario 1: cancellation**
 * User can tell a task_group object to cancel the tasks. After this, any tasks that use this
 * task_group object will not be executed anymore. Tasks that are in progress at the moment of
 * cancellation are not by default canceled. Instead, they can check from time to time whether the
 * task is canceled.
 *
 * If a parent task_group is canceled, all the tasks belonging to the children task_group objects
 * are canceled.
 *
 * **Scenario 2: waiting for tasks**
 * A task_group object can be queried to check if all the tasks in a task_group are done executing.
 * Actually, we are checking whether they are still active (the distinction is small, but can be
 * important in some cases). Whenever all the tasks are done executing (they don't reference the
 * task_group anymore) then the task_group object can tell that. One can easily query this property
 * by calling @ref is_active()
 *
 * Also, one can spawn a certain number of tasks, associating a task_group object with them and
 * wait for all these tasks to be completed by waiting on the task_group object. This is an active
 * wait: the thread tries to execute tasks while waiting (with the idea that it will try to speed
 * up the completion of the tasks) -- the waiting algorithm can vary based on other factors.
 *
 * **Scenario 3: Exception handling**
 * One can set an exception handler to the task_group. If a task throws an exception, and the
 * associated task_group has a handler set, then the handler will be called. This can be useful to
 * keep track of exceptions thrown by tasks. For example, one might want to add logging for thrown
 * exceptions.
 *
 * @see task
 */
class task_group {
public:
    /**
     * @brief      Default constructor
     *
     * @details
     *
     * Creates an empty, invalid task_group. No operations can be called on it. This is used to mark
     * the absence of a real task_group.
     *
     * @see create()
     */
    task_group();
    //! Destructor.
    ~task_group();

    /**
     * @brief      Copy constructor
     *
     * @details
     *
     * Creates a shared-copy of this object. The new object and the old one will share the same
     * implementation data.
     */
    task_group(const task_group&) = default;
    //! Move constructor; rarely used
    task_group(task_group&&) = default;
    /**
     * @brief      Assignment operator.
     *
     * @return     The result of the assignment
     *
     * @details
     *
     * Creates a shared-copy of this object. The new object and the old one will share the same
     * implementation data.
     */
    task_group& operator=(const task_group&) = default;
    //! Move assignment; rarely used
    task_group& operator=(task_group&&) = default;

    /**
     * @brief      Creates a task_group object, with an optional parent
     *
     * @param      parent  The parent of the task_group object (optional)
     *
     * @return     The task group created.
     *
     * @details
     *
     * As opposed to a default constructor, this creates a valid task_group object. Operations
     * (canceling, waiting) can be performed on objects created by this function.
     *
     * The optional `parent` parameter allows one to create hierarchies of task_group objects. A
     * hierarchy can be useful, for example, for canceling multiple groups of tasks all at once.
     *
     * @see task_group()
     */
    static task_group create(const task_group& parent = {});

    /**
     * @brief      Checks if this is a valid task group object
     *
     * @details
     *
     * Returns `true` if this object was created by @ref create() or if it's a copy of an object
     * created by calling @ref create().
     *
     * Such an object is *valid*, and operations can be made on it. Tasks will register into it and
     * they can be influenced by the task_group object.
     *
     * An object for which this returns `false` is considered invalid. It indicates the absence of a
     * real task_group object.
     *
     * @see create(), task_group()
     */
    explicit operator bool() const { return static_cast<bool>(impl_); }

    /**
     * @brief      Set the function to be called whenever an exception is thrown by a task.
     *
     * @param      except_fun  The function to be called on exceptions
     *
     * @details
     *
     * On execution, tasks can throw exceptions. If tasks have an associated task_group, one can use
     * this function to register an exception handler that will be called for exceptions.
     *
     * The given exception function will be called each time a new exception is thrown by a task
     * belonging to this task_group object.
     */
    void set_exception_handler(except_fun_t except_fun);

    /**
     * @brief      Cancels the execution tasks in the group.
     *
     * @details
     *
     * All tasks from this task group scheduled for execution that are not yet started are canceled
     * -- they won't be executed anymore. If there are tasks of this group that are in execution,
     * they can continue execution until the end. However, they have ways to check if the task group
     * is canceled, so that they can stop prematurely.
     *
     * Tasks that are added to the group after the group was canceled will be not executed.
     *
     * To get rid of the cancellation, one can cal clear_cancel().
     *
     * @see clear_cancel(), is_cancelled()
     */
    void cancel();

    /**
     * @brief      Clears the cancel flag; new tasks can be executed again.
     *
     * @details
     *
     * This reverts the effect of calling @ref cancel(). Tasks belonging to this group can be
     * executed once more after clear_clance() is called.
     *
     * Note, once individual tasks were decided that are canceled and not executed, this
     * @ref clear_cancel() cannot revert that. Those tasks will be forever not-executed.
     *
     * @see cancel(), is_cancelled()
     */
    void clear_cancel();

    /**
     * @brief      Checks if the tasks overseen by this object are canceled
     *
     * @return     True if the task group is canceled, False otherwise.
     *
     * @details
     *
     * This will return `true` after @ref cancel() is called, and `false` if @ref clear_cancel() is
     * called. If this return `true` it means that tasks belonging to this group will not be
     * executed.
     *
     * @ref cancel(), clear_cancel()
     */
    bool is_cancelled() const;

    /**
     * @brief      Checks whether there are active tasks in this group.
     *
     * @return     True if active, False otherwise.
     *
     * @details
     *
     * Creating one task into the task group will make the task group active, regardless of whether
     * the task is executed or not. The group will become non-active whenever all the tasks created
     * in the group are destroyed.
     *
     * One main assumption of task_groups is that, if a task is created in a task_group, then it is
     * somehow on the path to execution (i.e., enqueued in some sort of executor).
     *
     * This can be used to check when all tasks from a group are completed.
     *
     * If a group has sub-groups which have active tasks, this will return true.
     *
     * @see task
     */
    bool is_active() const;

    /**
     * @brief      Returns the task_group object for the current running task
     *
     * @return     The task group associated with the current running task
     *
     * @details
     *
     * If there is no task running, this will return an empty (i.e., default-constructed) object. If
     * there is a running task on this thread, it will return the task_group object for the
     * currently running task.
     *
     * The intent of this function is to be called from within running tasks.
     *
     * This uses thread-local-storage to store the task_group of the current running task.
     *
     * @see is_current_task_cancelled(), task_group()
     */
    static task_group current_task_group();

    /**
     * @brief      Determines if current task cancelled.
     *
     * @return     True if current task cancelled, False otherwise.
     *
     * @details
     *
     * This should be called from within tasks to check if the task_group associated with the
     * current running task was cancelled.
     *
     * The intent of this function is to be called from within running tasks.
     *
     * @see current_task_group()
     */
    static bool is_current_task_cancelled();

    /**
     * @brief      Sets the task group for the current worker.
     *
     * @param      grp   The new group to be set for the current worker.
     *
     * @return     The previous set task group (if any).
     *
     * @details
     *
     * This is used by implementation of certain algorithms to speed up the use of task groups. Not
     * intended to be heavily used. To be used with care.
     *
     * In general, after setting a task group, one may want to restore the old task group. This is
     * why the function returns the previous task_group object.
     */
    static task_group set_current_task_group(const task_group& grp);

private:
    //! Implementation detail of a task group object. Note that the implementation details can be
    //! shared between multiple task_group objects.
    std::shared_ptr<detail::task_group_impl> impl_;

    friend detail::task_group_access;
};

} // namespace v1

} // namespace concore