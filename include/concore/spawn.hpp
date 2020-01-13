#pragma once

#include "task.hpp"
#include "detail/task_system.hpp"

namespace concore {

namespace detail {

//! Defines an executor that can spawn tasks in the current worker queue.
struct spawn_executor {
    void operator()(task t) const { detail::task_system::instance().spawn(std::move(t)); }
};

} // namespace detail

inline namespace v1 {

//! Spawns a new task in the current worker thread.
//! It is assumed this is going to be called from within a task (within a worker thread); in this
//! case, the task will be added to the list of tasks for the current worker.
inline void spawn(task&& t) { detail::task_system::instance().spawn(std::move(t)); }

//! Executor that spawns tasks instead of enqueueing them
constexpr auto spawn_executor = detail::spawn_executor{};

} // namespace v1

} // namespace concore