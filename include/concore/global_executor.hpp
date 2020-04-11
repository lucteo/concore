#pragma once

#include "detail/task_system.hpp"

namespace concore {

namespace detail {

//! Structure that defines an executor for a given priority
template <task_priority P = task_priority::normal>
struct executor_with_prio {
    void operator()(task t) const { task_system::instance().enqueue<int(P)>(std::move(t)); }
};

} // namespace detail

inline namespace v1 {

/**
 * @brief      The default global executor.
 *
 * This is an executor that passes the tasks directly to concore's task system. Whenever there is a
 * core available, the task is executed.
 *
 * Is is the default executor.
 *
 * The tasks enqueued here are considered to have the priority "normal".
 *
 * @see global_executor_critical_prio, global_executor_high_prio, global_executor_normal_prio,
 *      global_executor_low_prio, global_executor_background_prio
 */
constexpr auto global_executor = detail::executor_with_prio<detail::task_priority::normal>{};

/**
 * @brief      Task executor that enqueues tasks with *critical* priority.
 * 
 * Similar to @ref global_executor, but the task is considered to have the *critical* priority.
 * Tasks with critical priority take precedence over all other types of tasks in the task system.
 * 
 * @see global_executor, global_executor_high_prio, global_executor_normal_prio,
 *      global_executor_low_prio, global_executor_background_prio
 */
constexpr auto global_executor_critical_prio =
        detail::executor_with_prio<detail::task_priority::critical>{};

/**
 * @brief      Task executor that enqueues tasks with *high* priority.
 * 
 * Similar to @ref global_executor, but the task is considered to have the *high* priority.
 * Tasks with high priority take precedence over normal priority tasks.
 * 
 * @see global_executor, global_executor_critical_prio, global_executor_normal_prio,
 *      global_executor_low_prio, global_executor_background_prio
 */
constexpr auto global_executor_high_prio =
        detail::executor_with_prio<detail::task_priority::high>{};

/**
 * @brief      Task executor that enqueues tasks with *normal* priority.
 * 
 * Same as global_executor.
 * 
 * @see global_executor, global_executor_critical_prio, global_executor_high_prio,
 *      global_executor_low_prio, global_executor_background_prio
 */
constexpr auto global_executor_normal_prio =
        detail::executor_with_prio<detail::task_priority::normal>{};

/**
 * @brief      Task executor that enqueues tasks with *low* priority.
 *
 * Similar to @ref global_executor, but the task is considered to have the *low* priority.
 * Tasks with low priority are executed after tasks of normal priority.
 *
 * @see global_executor, global_executor_critical_prio, global_executor_high_prio,
 *      global_executor_normal_prio, global_executor_background_prio
 */
constexpr auto global_executor_low_prio = detail::executor_with_prio<detail::task_priority::low>{};

/**
 * @brief      Task executor that enqueues tasks with *background* priority.
 *
 * Similar to @ref global_executor, but the task is considered to have the *background* priority.
 * Tasks with background are executed after all the other tasks in the system
 *
 * @see global_executor, global_executor_critical_prio, global_executor_high_prio,
 *      global_executor_normal_prio, global_executor_low_prio
 */
constexpr auto global_executor_background_prio =
        detail::executor_with_prio<detail::task_priority::background>{};

} // namespace v1
} // namespace concore