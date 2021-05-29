/**
 * @file    inline_executor.hpp
 * @brief   Defines the @ref concore::v1::inline_executor "inline_executor" class
 */
#pragma once

#include "task.hpp"

namespace concore {
inline namespace v1 {
/**
 * @brief Executor type that executes the work inline
 *
 * @details
 *
 * Whenever `execute` is called with a functor, the functor is directly called.
 * The calling party will be blocked until the functor finishes execution.
 *
 * Two objects of this type will always compare equal.
 */
struct inline_executor {
    //! Method called to execute work in this executor
    template <typename F>
    void execute(F&& f) const {
        task t{std::forward<F>(f)};
        t();
    }
    //! \overload
    void execute(task t) const { t(); }

    //! Equality operator; always true
    friend inline bool operator==(inline_executor, inline_executor) { return true; }
    //! Inequality operator; always false
    friend inline bool operator!=(inline_executor, inline_executor) { return false; }
};

} // namespace v1
} // namespace concore