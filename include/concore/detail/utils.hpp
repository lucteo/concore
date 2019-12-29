#pragma once

#include "../task.hpp"
#include "concurrent_queue.hpp"

#include <functional>
#include <cassert>

namespace concore {

namespace detail_shared {

//! Pops one task from the given task queue and executes it.
//! If the execution throws an exception, call the given except fun
void pop_and_execute(
        concurrent_queue<task>& q, std::function<void(std::exception_ptr)> except_fun) {
    task to_execute;
    bool res = q.try_pop(to_execute);
    assert(res);

    try {
        to_execute();
    } catch (...) {
        if (except_fun)
            except_fun(std::current_exception());
    }
}

} // namespace detail_shared
} // namespace concore