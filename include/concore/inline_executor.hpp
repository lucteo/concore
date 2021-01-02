#pragma once

#include "task.hpp"

namespace concore {
inline namespace v1 {
/**
 * @brief Executor type that executes the work inline
 *
 * @param f Functor to be executed
 *
 * Whenever `execute` is called with a functor, the functor is directly called.
 * The calling party will be blocked until the functor finishes execution.
 *
 * Two objects of this type will always compare equal.
 */
struct inline_executor {
    template <typename F>
    void execute(F&& f) const {
        std::forward<F>(f)();
    }

    // TODO (now): Remove this
    template <typename F>
    void operator()(F&& f) const noexcept(noexcept(std::forward<F>(f)())) {
        std::forward<F>(f)();
    }

    friend inline bool operator==(inline_executor, inline_executor) { return true; }
    friend inline bool operator!=(inline_executor, inline_executor) { return false; }
};

} // namespace v1
} // namespace concore