#pragma once

#include "../task.hpp"
#include "../data/concurrent_queue.hpp"
#include "../low_level/spin_backoff.hpp"

#include <functional>
#include <cassert>

namespace concore {

namespace detail_shared {

//! Pops one task from the given task queue and executes it.
//! If the execution throws an exception, call the given except fun
template <typename Q>
inline void pop_and_execute(Q& q, std::function<void(std::exception_ptr)> except_fun) {
    task to_execute;
    // In the vast majority of cases there is a task ready to be executed; but there can be some
    // race conditions that will prevent the first pop from extracting a task from the queue. In
    // this case, try to spin for a bit
    spin_backoff spinner;
    while (!q.try_pop(to_execute))
        spinner.pause();

    try {
        to_execute();
    } catch (...) {
        if (except_fun)
            except_fun(std::current_exception());
    }
}
} // namespace detail_shared
} // namespace concore