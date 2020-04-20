#pragma once

#include "../executor_type.hpp"
#include "../except_fun_type.hpp"

#include <functional>

namespace concore {
namespace detail {

//! Called to enqueue the "next" task. Handles exceptions thrown by the executor.
inline void enqueue_next(executor_t& executor, task&& t, except_fun_t except_fun) noexcept {
    try {
        executor(std::move(t));
    } catch (...) {
        except_fun(std::current_exception());
    }
}

} // namespace detail
} // namespace concore