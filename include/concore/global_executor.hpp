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

//! The global task executor. This will enqueue a task with a "normal" priority in the system, and
//! whenever we have a core available to execute it, it will be executed.
constexpr auto global_executor = detail::executor_with_prio<detail::task_priority::normal>{};

//! Task executor that enqueues tasks with "critical" priority.
constexpr auto global_executor_critical_prio =
        detail::executor_with_prio<detail::task_priority::critical>{};
//! Task executor that enqueues tasks with "high" priority.
constexpr auto global_executor_high_prio =
        detail::executor_with_prio<detail::task_priority::high>{};
//! Task executor that enqueues tasks with "normal" priority. Same as `global_executor`.
constexpr auto global_executor_normal_prio =
        detail::executor_with_prio<detail::task_priority::normal>{};
//! Task executor that enqueues tasks with "low" priority.
constexpr auto global_executor_low_prio = detail::executor_with_prio<detail::task_priority::low>{};
//! Task executor that enqueues tasks with "background" priority.
constexpr auto global_executor_background_prio =
        detail::executor_with_prio<detail::task_priority::background>{};

} // namespace v1
} // namespace concore