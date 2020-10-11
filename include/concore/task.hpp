#pragma once

#include "task_group.hpp"

#include <functional>

namespace concore {

namespace detail {
class exec_context;
}

inline namespace v1 {

/**
 * A function type that is compatible with a task.
 *
 * This function takes no arguments and returns nothing. It represents generic *work*.
 *
 * A concore @ref task is essentially a wrapper over a `task_function`.
 *
 * @see task
 */
using task_function = std::function<void()>;

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
     * @brief      Constructs a new task given a functor
     *
     * @param      ftor  The functor to be called when executing task.
     *
     * @tparam     T     The type of the functor. Must be compatible with @ref task_function.
     *
     * When the task will be executed, the given functor will be called. This typically happens on
     * a different thread than this constructor is called.
     *
     * To be assumed that the functor will be called at a later time. All the resources needed by
     * the functor must be valid at that time.
     */
    template <typename T>
    // cppcheck-suppress noExplicitConstructor
    task(T ftor)
        : fun_(std::move(ftor)) {}

    /**
     * @brief      Constructs a new task given a functor and a task group
     *
     * @param      ftor  The functor to be called when executing task.
     * @param      grp   The task group to place the task in the group.
     *
     * @tparam     T     The type of the functor. Must be compatible with @ref task_function.
     *
     * When the task will be executed, the given functor will be called. This typically happens on
     * a different thread than this constructor is called.
     *
     * To be assumed that the functor will be called at a later time. All the resources needed by
     * the functor must be valid at that time.
     *
     * Through the given group one can cancel the execution of the task, and check (indirectly) when
     * the task is complete.
     *
     * After a call to this constructor, the group becomes "active". Calling @ref
     * task_group::is_active() will return true.
     *
     * @see get_task_group()
     */
    template <typename T>
    task(T ftor, task_group grp)
        : fun_(std::move(ftor))
        , task_group_(grp) {
        detail::task_group_access::on_task_created(grp);
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
     */
    ~task() { detail::task_group_access::on_task_destroyed(task_group_); }

    //! Move constructor
    task(task&&) = default;
    //! Move operator
    task& operator=(task&&) = default;

    //! Copy constructor is DISABLED
    task(const task&) = delete;
    //! Copy assignment operator is DISABLED
    task& operator=(const task&) = delete;

    /**
     * @brief      Swap the content of the task with another task
     *
     * @param      other  The other task
     */
    void swap(task& other) {
        fun_.swap(other.fun_);
        std::swap(task_group_, other.task_group_);
    }

    /**
     * @brief      Bool conversion operator.
     *
     * Indicates if a valid functor is set into the tasks, i.e., if there is anything to be
     * executed.
     */
    explicit operator bool() const noexcept { return static_cast<bool>(fun_); }

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
     */
    void operator()() { fun_(); }

    /**
     * @brief      Gets the task group.
     *
     * @return     The group associated with this task.
     *
     * This allows the users to consult the task group associated with the task and change it.
     */
    const task_group& get_task_group() const { return task_group_; }

private:
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
     * The group that this tasks belongs to.
     *
     * This can be set by the constructor, or can be set by calling @ref get_task_group().
     * As the library prefers passing tasks around by moving them, after the task was enqueued, the
     * task group cannot be changed.
     */
    task_group task_group_;
};
} // namespace v1

} // namespace concore