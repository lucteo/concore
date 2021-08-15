/**
 * @file    task.hpp
 * @brief   Defines the @ref concore::v1::task "task" class
 *
 * @see     @ref concore::v1::task "task"
 */
#pragma once

#include "task_group.hpp"

#include <functional>
#include <cassert>

namespace concore {

namespace detail {
//! Wrapper that ensures that the group is notified whenever the task contains the group.
struct task_group_wrapper {
    task_group grp_{};

    task_group_wrapper() = default;
    explicit task_group_wrapper(task_group&& grp)
        : grp_(grp) {
        detail::task_group_access::on_task_created(grp_);
    }
    ~task_group_wrapper() { detail::task_group_access::on_task_destroyed(grp_); }

    task_group_wrapper(task_group_wrapper&&) = default;
    task_group_wrapper& operator=(task_group_wrapper&&) = default;

    task_group_wrapper(const task_group_wrapper& other)
        : grp_(other.grp_) {
        detail::task_group_access::on_task_created(grp_);
    }
    task_group_wrapper& operator=(const task_group_wrapper& other) {
        grp_ = other.grp_;
        detail::task_group_access::on_task_created(grp_);
        return *this;
    }

    const task_group& get_task_group() const noexcept { return grp_; }

    // void set_task_group(task_group grp) { return grp_; }
};
} // namespace detail

inline namespace v1 {

/**
 * @brief   A function type that is compatible with a task.
 *
 * This function takes no arguments and returns nothing. It represents generic *work*.
 *
 * A concore @ref concore::v1::task "task" is essentially a wrapper over a `task_function`.
 *
 * @see task
 */
using task_function = std::function<void()>;

/**
 * @brief   The type of function called at the end of the task.
 *
 * @param   _ The exception seen in the task, or empty object
 *
 * @details
 *
 * This allows the task-manipulation structures (pipelines, serializers, etc.) to be notified at the
 * completion of the task, and perform continuation actions on them.
 *
 * If a continuation function will be set to a task, then that function will be called no matter how
 * the task execution is ending (successfully, with error, cancellation).
 *
 * If the task is finished successfully, this will be called with an empty exception ptr. If the
 * task finishes with an exception, then that exception will be passed here. If the task has been
 * cancelled and never started executing, then this will be called with a `task_cancelled`
 * exception.
 *
 * @see task, task_cancelled
 */
using task_continuation_function = std::function<void(std::exception_ptr)>;

/**
 * @brief      A task. Core abstraction for representing an independent unit of work.
 *
 * A task can be enqueued into an *executor* and executed at a later time. That is, this represents
 * work that can be scheduled.
 *
 * Tasks have move-only semantics, and disable copy semantics. Also, the library prefers to move
 * tasks around instead of using shared references to the task. That means that, after construction
 * and initialization, once passed to an executor, the task cannot be modified.
 *
 * It is assumed that a task can only be executed once.
 *
 * **Ensuring correctness when working with tasks**
 *
 * Within the *independent unit of work* definition, the word **independent** is crucial. It is the
 * one that guarantees thread-safety of the applications relying on tasks to represent concurrency.
 *
 * A task needs to be independent, meaning that **it must not be run in parallel with other tasks
 * that touch the same data** (and one of them is writing). In other words, it enforces no data
 * races, and no corruptions (in this context data races have the negative effect and they represent
 * undefined behavior).
 *
 * Please note that this does not say that there can't be two tasks that touch the same data (and
 * one of them is writing). It says that if we have such case we need to ensure that these tasks are
 * not running in parallel. In other words, one need to apply *constraints* on these tasks to ensure
 * that they re not run in parallel.
 *
 * If constraints are added on the tasks ensuring that there are no two conflicting tasks that run
 * in parallel, then we can achieve concurrency without data races.
 *
 * At the level of @ref task object, there is no explicit way of adding constraints on the tasks.
 * The constraints can be added on top of tasks. See @ref chained_task and @ref serializer.
 *
 * **task_function and task_group**
 *
 * A task is essentially a pair of a `task_function` an a @ref task_group. The `task_function` part
 * offers storage for the work associated with the task. The @ref task_group part offers a way of
 * controlling the task execution.
 *
 * One or most tasks can belong to a task_group. To add a task to an existing task_group pass the
 * task_group object to the constructor of the task object. By using a task_group the user can tell
 * the system to cancel the execution of all the tasks that belong to the task_group. It can also
 * implement logic that depends on the the task_group having no tasks attached to it.
 *
 * @see task_function, task_group, chained_task, serializer
 */
class task {
public:
    /**
     * @brief      Default constructor
     *
     * Brings the task into a valid state. The task has no action to be executed, and does not
     * belong to any task group.
     */
    task() = default;

    /**
     * @brief      Constructs a new task given a functor and a task group
     *
     * @param      ftor The functor to be called when executing task.
     * @param      grp  The task group to place the task in the group.
     * @param      cont The functor to be called when the task is finished
     *
     * @tparam     F    The type of the functor. Must be compatible with `task_function`.
     * @tparam     CF   The type of the continuation functor. Must be compatible with
     *                  `task_continuation_function`.
     *
     * @details
     *
     * When the task will be executed, the given functor will be called. This typically happens on
     * a different thread than this constructor is called.
     *
     * To be assumed that the functor will be called at a later time. All the resources needed by
     * the functor must be valid at that time.
     *
     * If a `task_group` is given, then through that group one can cancel the execution of the task,
     * and check (indirectly) when the task is complete. After a call to this constructor, the group
     * becomes "active". Calling
     * @ref task_group::is_active() will return true.
     *
     * If a continuation function is given, then that function will be called regardless on how the
     * task is executed (assuming that the task is queued for execution). It is called after a
     * successful execution, it is called if the task throws an exception, and it is called if the
     * task is cancelled.
     *
     * @see get_task_group()
     */
    template <typename F, typename CF>
    task(F ftor, task_group grp, CF cont)
        : task_group_(std::move(grp))
        , fun_(std::move(ftor))
        , cont_fun_(std::move(cont)) {
        assert(fun_);
    }
    //! @overload
    template <typename F>
    explicit task(F ftor)
        : fun_(std::move(ftor)) {
        assert(fun_);
    }
    //! @overload
    template <typename F>
    task(F ftor, task_group grp)
        : task_group_(std::move(grp))
        , fun_(std::move(ftor)) {
        assert(fun_);
    }

    /**
     * @brief      Assignment operator from a functor
     *
     * @param      ftor  The functor to be called when executing task.
     *
     * @tparam     T     The type of the functor. Must be compatible with @ref task_function.
     *
     * @return     The result of the assignment
     *
     * @details
     *
     * This can be used to change the task function inside the task.
     */
    template <typename T>
    task& operator=(T ftor) {
        fun_ = std::move(ftor);
        return *this;
    }

    /**
     * @brief      Destructor.
     *
     * If the task belongs to the group, the group will contain one less active task. If this was
     * the last task registered in the group, after this call, calling @ref task_group::is_active()
     * will yield false.
     *
     * We ensure that both functor objects are destroyed before the group can be marked inactive.
     */
    ~task() = default;

    //! Move constructor
    task(task&&) = default;
    //! Move operator
    task& operator=(task&&) = default;

    //! Copy constructor
    task(const task&) = default;
    //! Copy assignment operator
    task& operator=(const task&) = default;

    /**
     * @brief      Swap the content of the task with another task
     *
     * @param      other  The other task
     */
    void swap(task& other) {
        fun_.swap(other.fun_);
        cont_fun_.swap(other.cont_fun_);
        std::swap(task_group_, other.task_group_);
    }

    /**
     * @brief      Function call operator; performs the action stored in the task.
     *
     * This is called by the execution engine whenever the task is ready to be executed. It will
     * call the functor stored in the task.
     *
     * The functor can throw, and the execution system is responsible for catching the exception and
     * ensuring its properly propagated to the user.
     *
     * This is typically called after some time has passed since task creation. The user must ensure
     * that the functor stored in the task is safe to be executed at that point.
     *
     * This does not invalidate the task object, by itself. Theoretically, this can be called
     * multiple times in a row.
     */
    void operator()() noexcept;

    /**
     * @brief Gets the continuation function stored in this task (may be empty)
     * @details
     *
     * This can be used to inspect the continuation of a task, with the goal of changing it.
     * If a task has a continuation set, then that continuation need to be executed, regardless of
     * whether the status is successfully executed or not. This means, that, if the continuation is
     * exchanged, the old continuation needs to be stored somewhere.
     *
     * @see     set_continuation()
     */
    task_continuation_function get_continuation() const noexcept { return cont_fun_; }

    /**
     * @brief Sets the continuation function for this task
     *
     * @param cont The continuation function to be associated with the task
     *
     * @details
     *
     * This can be called to add continuation actions to a task that is executing. Please note that
     * the old continuation function (if set), must be executed. This means that
     * `get_continuation()` need to be called whenever this is called.
     *
     * @see     get_continuation()
     */
    void set_continuation(task_continuation_function cont) { cont_fun_ = std::move(cont); }

    /**
     * @brief      Gets the task group.
     *
     * @return     The group associated with this task.
     *
     * @details
     *
     * This allows the users to consult the task group associated with the task.
     */
    const task_group& get_task_group() const { return task_group_.get_task_group(); }

    /**
     * @brief Sets the task group for the task
     * @param grp The group that we want to set
     *
     * @details
     *
     * This is useful when manipulating tasks. One should not change the task
     * group for tasks that are in execution.
     */
    void set_task_group(task_group grp) noexcept {
        task_group_ = detail::task_group_wrapper(std::move(grp));
    }

    /**
     * @brief Returns the current executing task (if any)
     * @details
     *
     * If we are within the execution of a task, this will return the current task object being
     * executed. Otherwise this will return nullptr.
     *
     * @see     exchange_continuation()
     */
    static task* current_task();

private:
    /**
     * The group that this tasks belongs to.
     *
     * This can be set by the constructor, or can be set by calling @ref set_task_group().
     * As the library prefers passing tasks around by moving them, after the task was enqueued, the
     * task group cannot be changed.
     *
     * This object is wrapped so that we notify the group whenever this task joins or leaves the
     * group. We need to make sure that we exit the group only after both functors are destroyed.
     */
    detail::task_group_wrapper task_group_;

    /**
     * The function to be called.
     *
     * This can be associated with a task through construction and by using the special assignment
     * operator.
     *
     * Please note that, as the tasks cannot be copied and shared, and as the task system prefers
     * moving tasks, after the task is enqueued this is constant.
     */
    task_function fun_;

    /**
     * @brief Function to be called after finishing the execution of the main function.
     *
     * This can be used by structures like pipelines or serializers to execute continuations after
     * the task job is completed.
     *
     * This is guaranteed to be called for any enqueued task, regardless whether the task was
     * executed or not, regardless whether the task has finished successfully or with an exception.
     */
    task_continuation_function cont_fun_;
};

/**
 * @brief   Exchange the continuation of the currently running task.
 *
 * @param   new_cont The new continuation to be set to the current task; default: no continuation
 *
 * @return  The continuation that was attached to the current task.
 * @details
 *
 * This can be used to break the current task into a set of new tasks, by transferring the
 * continuation of the current task to another task.
 *
 * If there is no active running task, this will return the input argument.
 *
 * @warning     Please ensure that the continuation is always called, especially in the presence of
 *              errors. Failure to do so, might stop some concurrent abstractions to work.
 */
task_continuation_function exchange_cur_continuation(task_continuation_function&& new_cont = {});

} // namespace v1

} // namespace concore