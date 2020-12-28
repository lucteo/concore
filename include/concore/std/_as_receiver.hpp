#pragma once

#include <concore/detail/concept_macros.hpp>
#include <exception>

namespace concore {

namespace std_execution {

inline namespace v1 {

/**
 * @brief   Wrapper that transforms a functor into a receiver
 *
 * @tparam  F The type of the functor
 *
 * This will implement the operations specific to a receiver given a functor. The receiver will call
 * the functor whenever `set_value()` is called. It will not do anything on set_done() and it will
 * terminate the program if set_error() is called.
 */
template <typename F>
struct as_receiver {
    explicit as_receiver(F&& f)
        : f_((F &&) f) {}

    //! Called whenever the sender completed the work with success
    void set_value() noexcept(CONCORE_DECLVAL(F)()) { f_(); }
    //! Called whenever the work was canceled
    void set_done() noexcept {}
    //! Called whenever there was an error while performing the work in the sender.
    template <typename E>
    [[noreturn]] void set_error(E) noexcept {
        std::terminate();
    }

private:
    //! The functor to be called when the sender finishes the work
    F f_;
};

} // namespace v1

} // namespace std_execution
} // namespace concore
