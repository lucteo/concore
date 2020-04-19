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
 * task_group anymore) then the task_group object can tell that. One can easily query this.
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
     * On execution, tasks can throw exceptions. If tasks have an associated task_group, one can use
     * this function to register an exception handler that will be called for exceptions.
     * 
     * The given exception function will be called each time a new exception is thrown by a task
     * belonging to this task_group object.
     */
    void set_exception_handler(std::function<void(std::exception_ptr)> except_fun);

    /**
     * @brief      Cancels the execution tasks in the group.
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
     * This will return `true` after @ref cancel() is called, and `false` if @ref clear_cancel() is
     * called. If this return `true` it means that tasks belonging to this group will not be
     * executed.
     * 
     * @ref cancel(), clear_cancel()
     */
    bool is_cancelled() const;

    /**
     * @brief      Checks whether there are tasks or other task_group objects in this group
     *
     * @return     True if active, False otherwise.
     * 
     * This can be used to check when all tasks from a group are completed.
     * Completed tasks are not stored by the task system, nor by the executors, so they release the
     * reference to this object. This function returns `true` if no tasks refer to this group.
     * 
     * For a newly created task_group, this will return `false`, as there are no tasks in it.
     * 
     * - TODO: Don't count subgroups
     * - TODO: make it work hierarchically.
     */
    bool is_active() const;

    /**
     * @brief      Returns the task_group object for the current running task
     *
     * @return     The task group associated with the current running task
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
     * This should be called from within tasks to check if the task_group associated with the
     * current running task was cancelled.
     * 
     * The intent of this function is to be called from within running tasks.
     *
     * @see current_task_group()
     */
    static bool is_current_task_cancelled();

private:
    //! Implementation detail of a task group object. Note that the implementation details can be 
    //! shared between multiple task_group objects.
    std::shared_ptr<detail::task_group_impl> impl_;

    friend detail::task_group_access;
};

} // namespace v1

} // namespace concore