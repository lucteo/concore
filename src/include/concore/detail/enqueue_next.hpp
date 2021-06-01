#pragma once

#include "../except_fun_type.hpp"

#include <functional>

namespace concore {
namespace detail {

//! Called to enqueue the "next" task. Handles exceptions thrown by the executor.
template <typename E>
inline void enqueue_next(E& executor, task&& t, except_fun_t except_fun) noexcept {
    try {
        executor.execute(std::move(t));
    } catch (...) {
        except_fun(std::current_exception());
    }
}

} // namespace detail
} // namespace concore