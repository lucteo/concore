/**
 * @file    delegating_executor.hpp
 * @brief   Defines the @ref concore::v1::delegating_executor "delegating_executor" class
 */
#pragma once

#include "task.hpp"
#include <functional>

namespace concore {
inline namespace v1 {

/**
 * @brief Executor type that forwards the execution to a give functor
 *
 * All the functor objects passed to the @ref execute() method will be delegated to a function that
 * takes a @ref task parameter. This function is passed to the constructor of the class.
 */
struct delegating_executor {
    //! The type of the function to be used to delegate execution to
    using fun_type = std::function<void(task)>;

    //! Constructor
    explicit delegating_executor(fun_type f)
        : fun_(std::move(f)) {}

    //! Method called to execute work in this executor
    void execute(task t) const { fun_(std::move(t)); }
    //! @overload
    template <typename F>
    void execute(F&& f) const {
        fun_(task{std::forward<F>(f)});
    }

    //! Equality operator; always false
    friend inline bool operator==(delegating_executor l, delegating_executor r) { return false; }
    //! Inequality operator; always true
    friend inline bool operator!=(delegating_executor l, delegating_executor r) { return true; }

private:
    //! The functor to which the calls are delegated
    fun_type fun_;
};

} // namespace v1
} // namespace concore