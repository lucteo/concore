#pragma once

#include "task.hpp"
#include "detail/exec_context.hpp"
#include "detail/library_data.hpp"

#include <initializer_list>

namespace concore {

namespace detail {

//! Defines an executor that can spawn tasks in the current worker queue.
struct spawn_executor {
    void operator()(task t) const { detail::get_exec_context().spawn(std::move(t)); }
};

//! Similar to a spawn_executor, but doesn't wake other workers
struct spawn_continuation_executor {
    void operator()(task t) const { detail::get_exec_context().spawn(std::move(t), false); }
};

} // namespace detail

inline namespace v1 {

/**
 * @brief      Spawns a task, hopefully in the current working thread.
 *
 * @param      t             The task to be spawned
 * @param      wake_workers  True if we should wake other workers for this task
 *
 * This is intended to be called from within a task. In this case, the task will be added to the
 * list of tasks for the current worker thread. The tasks will be added in the front of the list, so
 * it will be executed in front of others.
 *
 * The add-to-front strategy aims as improving locality of execution. We assume that this task is
 * closer to the current task than other tasks in the system.
 *
 * If the current running task does not finish execution after spawning this new task, it's advised
 * for the `wake_workers` parameter to be set to `true`. If, on the other hand, the current task
 * finishes execution after this, it may be best to not set `wake_workers` to `false` and thus try
 * to wake other threads. Waking up other threads can be an efficiency loss that we don't need if we
 * know that this thread is finishing soon executing the current task.
 *
 * Note that the given task ca take a task_group at construction. This way, the users can control
 * the groups of the spawned tasks.
 */
inline void spawn(task&& t, bool wake_workers = true) {
    detail::get_exec_context().spawn(std::move(t), wake_workers);
}

/**
 * @brief      Spawn one task, given a functor to be executed.
 *
 * @param      ftor          The ftor to be executed
 * @param      wake_workers  True if we should wake other workers for this task
 *
 * @tparam     F             The type of the functor
 *
 * This is similar to the @ref spawn(task&&, bool) function, but it takes directly a functor instead
 * of a task.
 *
 * If the current task has a group associated, the new task will inherit that group.
 *
 * @see spawn(task&&, bool)
 */
template <typename F>
inline void spawn(F&& ftor, bool wake_workers = true) {
    auto grp = task_group::current_task_group();
    detail::get_exec_context().spawn(task(std::forward<F>(ftor), grp), wake_workers);
}

/**
 * @brief      Spawn one task, given a functor to be executed.
 *
 * @param      ftor          The ftor to be executed
 * @param      grp           The group in which the task should be executed.
 * @param      wake_workers  True if we should wake other workers for this task
 *
 * @tparam     F             The type of the functor
 *
 * This is similar to the @ref spawn(task&&, bool) function, but it takes directly a functor and a
 * task group instead of a task.
 *
 * If the current task has a group associated, the new task will inherit that group.
 *
 * @see spawn(task&&, bool)
 */
template <typename F>
inline void spawn(F&& ftor, task_group grp, bool wake_workers = true) {
    detail::get_exec_context().spawn(task(std::forward<F>(ftor), std::move(grp)), wake_workers);
}

/**
 * @brief      Spawn multiple tasks, given the functors to be executed.
 *
 * @param      ftors         A list of functors to be executed
 * @param      wake_workers  True if we should wake other workers for the last task
 *
 * This is similar to the other two @ref spawn() functions, but it takes a series of functions to be
 * executed. Tasks will be created for all these functions and spawn accordingly.
 *
 * The `wake_workers` will control whether to wake threads for the last task or not. For the others
 * tasks, it is assumed that we always want to wake other workers to attempt to get as many tasks as
 * possible from the current worker task list.
 *
 * If the current task has a task group associated, all the newly created tasks will inherit that
 * group.
 *
 * @ref spawn(task&&, bool), spawn_and_wait()
 */
inline void spawn(std::initializer_list<task_function>&& ftors, bool wake_workers = true) {
    auto grp = task_group::current_task_group();
    int count = static_cast<int>(ftors.size());
    for (const auto& ftor : ftors) {
        // wake_workers applies only to the last element; otherwise pass true
        bool cur_wake_workers = (count-- > 0 || wake_workers);
        detail::get_exec_context().spawn(task(ftor, grp), cur_wake_workers);
    }
}

/**
 * @brief      Spawn multiple tasks, given the functors to be executed.
 *
 * @param      ftors         A list of functors to be executed
 * @param      grp           The group in which the functors are to be executed
 * @param      wake_workers  True if we should wake other workers for the last task
 *
 * This is similar to the other two @ref spawn() functions, but it takes a series of functions to be
 * executed. Tasks will be created for all these functions and spawn accordingly.
 *
 * The `wake_workers` will control whether to wake threads for the last task or not. For the others
 * tasks, it is assumed that we always want to wake other workers to attempt to get as many tasks as
 * possible from the current worker task list.
 *
 * @ref spawn(task&&, bool), spawn_and_wait()
 */
inline void spawn(
        std::initializer_list<task_function>&& ftors, task_group grp, bool wake_workers = true) {
    int count = static_cast<int>(ftors.size());
    for (const auto& ftor : ftors) {
        // wake_workers applies only to the last element; otherwise pass true
        bool cur_wake_workers = (count-- > 0 || wake_workers);
        detail::get_exec_context().spawn(task(ftor, grp), cur_wake_workers);
    }
}

/**
 * @brief      Spawn a task and wait for it.
 *
 * @param      ftor  The functor of the tasks to be spawned
 *
 * @tparam     F     The type of the functor.
 *
 * This function is similar to the @ref spawn() functions, but, after spawning, also waits for the
 * spawned task to complete. This wait is an active-wait, as it tries to execute other tasks. In
 * principle, the current thread executes the spawn task.
 *
 * This will create a new task group, inheriting from the task group of the currently executing task
 * and add the new task in this new group. The waiting is done on this new group.
 *
 * @see spawn()
 */
template <typename F>
inline void spawn_and_wait(F&& ftor) {
    auto& ctx = detail::get_exec_context();
    auto worker_data = ctx.enter_worker();

    auto grp = task_group::create(task_group::current_task_group());
    ctx.spawn(task(std::forward<F>(ftor), grp), false);
    ctx.busy_wait_on(grp);

    ctx.exit_worker(worker_data);
}

/**
 * @brief      Spawn multiple tasks and wait for them to complete
 *
 * @param      ftors         A list of functors to be executed
 * @param      wake_workers  True if we should wake other workers for the last task
 *
 * This is used when a task needs multiple things done in parallel.
 *
 * This function is similar to the @ref spawn() functions, but, after spawning, also waits for the
 * spawned tasks to complete. This wait is an active-wait, as it tries to execute other tasks. In
 * principle, the current thread executes the last of the spawned tasks.
 *
 * This will create a new task group, inheriting from the task group of the currently executing task
 * and add the new tasks in this new group. The waiting is done on this new group.
 */
inline void spawn_and_wait(std::initializer_list<task_function>&& ftors, bool wake_workers = true) {
    auto& ctx = detail::get_exec_context();
    auto worker_data = ctx.enter_worker();

    auto grp = task_group::create(task_group::current_task_group());
    int count = static_cast<int>(ftors.size());
    for (const auto& ftor : ftors) {
        bool cur_wake_workers = count-- > 0; // don't wake on the last task
        ctx.spawn(task(ftor, grp), cur_wake_workers);
    }
    ctx.busy_wait_on(grp);

    ctx.exit_worker(worker_data);
}

/**
 * @brief      Wait on all the tasks in the given group to finish executing.
 *
 * @param      grp   The task group to wait on
 *
 * The wait here is an active-wait. This will execute tasks from the task system in the hope that
 * the tasks in the group are executed faster.
 *
 * Using this inside active tasks is not going to block the worker thread and thus not degrade
 * performance.
 *
 * @warning    If one adds task in a group and never executes them, this function will block
 *             indefinitely.
 *
 * @see spawn(), spawn_and_wait()
 */
inline void wait(task_group& grp) {
    auto& ctx = detail::get_exec_context();
    auto worker_data = ctx.enter_worker();
    detail::get_exec_context().busy_wait_on(grp);
    ctx.exit_worker(worker_data);
}

/**
 * Executor that spawns tasks instead of enqueueing them.
 * Similar to calling @ref spawn() on the task.
 *
 * @see spawn(), spawn_continuation_executor, global_executor
 */
constexpr auto spawn_executor = detail::spawn_executor{};

/**
 * Executor that spawns tasks instead of enqueueing them, but not waking other workers.
 * Similar to calling `spawn(task, false)` on the task.
 *
 * @see spawn(), spawn_executor, global_executor
 */
constexpr auto spawn_continuation_executor = detail::spawn_continuation_executor{};

} // namespace v1

} // namespace concore