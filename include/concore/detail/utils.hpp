#pragma once

#include "../task.hpp"
#include "../data/concurrent_queue.hpp"
#include "../low_level/spin_backoff.hpp"

#include <functional>
#include <cassert>

namespace concore {
namespace detail {

//! Executed the given tasks. Take care of the task_control interactions
inline void execute_task(task& t) {
    auto& tc = t.get_task_control();

    // If the task is canceled, don't do anything
    if (tc && tc.is_cancelled())
        return;

    try {
        detail::task_control_access::on_starting_task(tc, t);
        t();
        detail::task_control_access::on_task_done(tc, t);
    } catch (...) {
        detail::task_control_access::on_task_exception(tc, t, std::current_exception());
    }
}

//! Pops one task from the given task queue and executes it.
//! Use task_control for controlling the execution of the task
template <typename Q>
inline void pop_and_execute(Q& q) {
    task to_execute;
    // In the vast majority of cases there is a task ready to be executed; but there can be some
    // race conditions that will prevent the first pop from extracting a task from the queue. In
    // this case, try to spin for a bit
    spin_backoff spinner;
    while (!q.try_pop(to_execute))
        spinner.pause();

    execute_task(to_execute);
}
} // namespace detail
} // namespace concore