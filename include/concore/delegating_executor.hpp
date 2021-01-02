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

    explicit delegating_executor(fun_type f)
        : fun_(std::move(f)) {}

    void execute(task t) const { fun_(std::move(t)); }
    template <typename F>
    void execute(F&& f) const {
        fun_(task{std::forward<F>(f)});
    }

    // TODO (now): Remove this
    template <typename F>
    void operator()(F&& f) const {
        execute(std::forward<F>(f));
    }

    friend inline bool operator==(delegating_executor l, delegating_executor r) { return false; }
    friend inline bool operator!=(delegating_executor l, delegating_executor r) { return true; }

private:
    //! The functor to which the calls are delegated
    fun_type fun_;
};

} // namespace v1
} // namespace concore