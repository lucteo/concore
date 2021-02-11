#pragma once

#include "../task.hpp"
#include "../data/concurrent_queue.hpp"
#include "../low_level/spin_backoff.hpp"

#include <functional>
#include <cassert>

namespace concore {
namespace detail {

//! Pops one task from the given task queue and returns it.
template <typename Q>
inline task pop_task(Q& q) {
    task to_execute;
    // In the vast majority of cases there is a task ready to be executed; but there can be some
    // race conditions that will prevent the first pop from extracting a task from the queue. In
    // this case, try to spin for a bit
    spin_backoff spinner;
    while (!q.try_pop(to_execute))
        spinner.pause();

    return to_execute;
}

//! Pops one task from the given task queue and executes it.
template <typename Q>
inline void pop_and_execute(Q& q) {
    auto t = pop_task(q);
    execute_task(t);
}
} // namespace detail
} // namespace concore