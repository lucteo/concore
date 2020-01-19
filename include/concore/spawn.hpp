#pragma once

#include "task.hpp"
#include "detail/task_system.hpp"

namespace concore {

namespace detail {

//! Defines an executor that can spawn tasks in the current worker queue.
struct spawn_executor {
    void operator()(task t) const { detail::task_system::instance().spawn(std::move(t)); }
};

//! Similar to a spawn_executor, but doesn't wake other workers
struct spawn_continuation_executor {
    void operator()(task t) const { detail::task_system::instance().spawn(std::move(t), false); }
};

} // namespace detail

inline namespace v1 {

//! Spawns a new task in the current worker thread.
//! It is assumed this is going to be called from within a task (within a worker thread); in this
//! case, the task will be added to the list of tasks for the current worker.
//!
//! If the 'wake_workers' parameter is true, this will attempt to wake up workers to ensure that the
//! task is executed as soon as possible. But, if this is "a continuation" of the parent task, this
//! may not make sense, as we are moving computation to a different thread; for such cases, one can
//! pass false to the wake_workers parameter.
inline void spawn(task&& t, bool wake_workers = true) {
    detail::task_system::instance().spawn(std::move(t), wake_workers);
}

//! Executor that spawns tasks instead of enqueueing them
constexpr auto spawn_executor = detail::spawn_executor{};
//! Executor that spawns tasks instead of enqueueing them; this doesn't wake up other workers, so
//! the task is assumed to be a continuation of the current work.
constexpr auto spawn_continuation_executor = detail::spawn_continuation_executor{};

} // namespace v1

} // namespace concore