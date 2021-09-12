/**
 * @file    as_receiver.hpp
 * @brief   Definition of @ref concore::v1::as_receiver "as_receiver"
 *
 * @see     @ref concore::v1::as_receiver "as_receiver"
 */
#pragma once

#include <concore/detail/concept_macros.hpp>
#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_done.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <exception>

namespace concore {
inline namespace v1 {

/**
 * @brief   Wrapper that transforms a functor into a receiver
 *
 * @tparam  F The type of the functor
 *
 * @details
 *
 * This will implement the operations specific to a receiver given a functor. The receiver will call
 * the functor whenever set_value() is called. It will not do anything on set_done() and it will
 * terminate the program if set_error() is called.
 *
 * This types models the `receiver_of<>` concept
 *
 * @see receiver_of, set_value(), set_done(), set_error()
 */
template <typename F>
struct as_receiver {
    //! Constructor
    explicit as_receiver(F&& f) noexcept
        : f_((F &&) f) {}

private:
    //! The functor to be called when the sender finishes the work
    F f_;

    friend void tag_invoke(set_value_t, const as_receiver& self) noexcept(noexcept(self.f_())) {
        self.f_();
    }
    friend void tag_invoke(set_value_t, as_receiver&& self) noexcept(
            noexcept(std::move(self).f_())) {
        std::move(self).f_();
    }
    friend void tag_invoke(set_done_t, const as_receiver& self) noexcept {}
    [[noreturn]] friend void tag_invoke(
            set_error_t, const as_receiver& self, std::exception_ptr e) noexcept {
        std::terminate();
    }
};

} // namespace v1
} // namespace concore
