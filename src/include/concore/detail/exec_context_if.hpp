#pragma once

#include "concore/detail/task_priority.hpp"

namespace concore {

inline namespace v1 {
class task;
class task_group;
} // namespace v1

namespace detail {

class exec_context;
struct worker_thread_data;

/**
 * @brief Enqueue a task in the execution context.
 *
 * @param ctx  The execution context object in which we enqueue the task
 * @param t    The task we want to be executed
 * @param prio The priority of the enqueued task
 *
 * This will add the task in the execution's context queue of tasks to be executed. As opposed to
 * spawning, the enqueuing tries to get the tasks executed in roughly the same order in which they
 * were added. Also, this supports adding tasks with different priorities.
 *
 * This is defined outside of the exec_context class, so that users don't have to include the
 * class header.
 *
 * This can throw. If that happens, the task object passed in is not moved from.
 *
 * @see  do_enqueue_noexcept() do_spawn(), exec_context
 */
void do_enqueue(exec_context& ctx, task&& t, task_priority prio = task_priority::normal);

/**
 * @brief Enqueue a task in the execution context (noexcept).
 *
 * @param ctx  The execution context object in which we enqueue the task
 * @param t    The task we want to be executed
 * @param prio The priority of the enqueued task
 *
 * This will add the task in the execution's context queue of tasks to be executed. As opposed to
 * spawning, the enqueuing tries to get the tasks executed in roughly the same order in which they
 * were added. Also, this supports adding tasks with different priorities.
 *
 * This is defined outside of the exec_context class, so that users don't have to include the
 * class header.
 *
 * This never throws. If there is an exception generated while trying to enqueue the task, then the
 * continuation function of the task will be called with the exception that occurred.
 *
 * @see  do_enqueue(), do_spawn(), exec_context
 */
void do_enqueue_noexcept(
        exec_context& ctx, task&& t, task_priority prio = task_priority::normal) noexcept;

/**
 * @brief Spawns a task in the execution context.
 *
 * @param ctx           The execution context object in which we spawn the task
 * @param t             The task to be spawned
 * @param wake_workers  True if we need to wake any workers for this
 *
 * As opposed to enqueuing tasks, this will add the tasks to the front of the queue, so that the
 * tasks will roughly executed in the LIFO order. The aim for this one is to maximize locality.
 *
 * If we are spawning a task at the end of another task, or when the current thread is ready to
 * execute new work, then it may be faster if we don't wake other worker threads; we know that the
 * current thread will be able to pick this task soon enough. In this case, pass `false` for the
 * @ref wake_workers parameter.
 *
 * This is defined outside of the exec_context class, so that users don't have to include the
 * class header.
 *
 * This can throw. If that happens, the task object passed in is not moved from.
 *
 * @see  do_enqueue(), exec_context
 */
void do_spawn(exec_context& ctx, task&& t, bool wake_workers = true);

/**
 * @brief Spawns a task in the execution context (noexcept).
 *
 * @param ctx           The execution context object in which we spawn the task
 * @param t             The task to be spawned
 * @param wake_workers  True if we need to wake any workers for this
 *
 * As opposed to enqueuing tasks, this will add the tasks to the front of the queue, so that the
 * tasks will roughly executed in the LIFO order. The aim for this one is to maximize locality.
 *
 * If we are spawning a task at the end of another task, or when the current thread is ready to
 * execute new work, then it may be faster if we don't wake other worker threads; we know that the
 * current thread will be able to pick this task soon enough. In this case, pass `false` for the
 * @ref wake_workers parameter.
 *
 * This is defined outside of the exec_context class, so that users don't have to include the
 * class header.
 *
 * This never throws. If there is an exception generated while trying to spawn the task, then the
 * continuation function of the task will be called with the exception that occurred.
 *
 * @see  do_enqueue(), exec_context
 */
void do_spawn_noexcept(exec_context& ctx, task&& t, bool wake_workers = true) noexcept;

/**
 * @brief Busy-wait until the given task group is not active anymore.
 *
 * @param ctx The execution context object that is executing tasks from the group
 * @param grp The task group we are waiting for
 *
 * This function will not return while the group still has active tasks. Instead, this will try to
 * execute tasks from the execution context, in the hope that the tasks in the group are executed
 * faster. Note that, the execution context do not know how to specifically execute the tasks from
 * the given group (those tasks may not even be in the execution context yet).
 *
 * @note The calling thread must be a worker thread of the execution context. An external thread can
 * always be added to the execution context with @ref enter_worker().
 *
 * This is defined outside of the exec_context class, so that users don't have to include the
 * class header.
 *
 * @see  exec_context
 */
void busy_wait_on(exec_context& ctx, task_group& grp);

/**
 * @brief Ensures that the caling thread is a part of the execution context
 *
 * @param ctx The execution context the thread needs to be part of
 * @return worker_thread_data* Opaque object containing the thread data
 *
 * If this is called from a thread that is already part of the execution context, this will return
 * nullptr and exit immediatelly.
 *
 * If this is called from a thread that is not part of the execution context, this will attempt to
 * add this thread to it, and return the corresponding worker_thread_data object.
 *
 * If the returned pointer is not null, the caller is repsonsible for calling @ref exit_worker()
 * whenever the thread should get out of the execution context.
 *
 * This is useful whenever we want a thread to join the execution context until we finish a group of
 * tasks.
 *
 * This is defined outside of the exec_context class, so that users don't have to include the
 * class header.
 *
 * @see exit_worker(), busy_wait_on(), spawn_and_wait(), wait()
 */
worker_thread_data* enter_worker(exec_context& ctx);

/**
 * @brief Tells that the current thread exists the execution context
 *
 * @param ctx           The execution context that the thread is exiting
 * @param worker_data   The worker thread that generated by @ref enter_worker()
 *
 * This should be called after a busy wait operation to notify the execution context that the thread
 * is leaving it. The worker_data perameter must correspond to the value returned by @ref
 * enter_worker().
 *
 * This is defined outside of the exec_context class, so that users don't have to include the
 * class header.
 *
 * @see enter_worker(), busy_wait_on()
 */
void exit_worker(exec_context& ctx, worker_thread_data* worker_data);

//! Returns the number of worker threads in the execution context
int num_worker_threads(const exec_context& ctx);

//! Tests if there are tasks currently executing in our execution context.
//! Note: the returned value can be highly volatile.
bool is_active(const exec_context& ctx);

/**
 * @brief Returns the number of tasks active in the execution context.
 *
 * @param ctx  The execution context object to query
 * @return int The number of active tasks.
 *
 * This will count only the tasks that were added to the execution context (through @ref
 * do_enqueue() or @ref spawn()) and are not yet finished executing. It dos not include any tasks
 * that are not yet added to the execution context, like the tasks that are on the waiting lists of
 * serializers and similar structures.
 *
 * This is defined outside of the exec_context class, so that users don't have to include the
 * class header.
 *
 * @see is_active()
 */
int num_active_tasks(const exec_context& ctx);

// Functions in this header are implemented in exec_context.cpp

} // namespace detail

} // namespace concore