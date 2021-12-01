/**
 * @file    just_done.hpp
 * @brief   Definition of @ref concore::v1::just_done "just_done"
 *
 * @see     @ref concore::v1::just_done "just_done"
 */
#pragma once

#include <concore/detail/extra_type_traits.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <concore/_cpo/_cpo_connect.hpp>
#include <concore/_cpo/_cpo_start.hpp>
#include <concore/_concepts/_concepts_receiver.hpp>

#include <exception>

namespace concore {

namespace detail {

struct just_done_sender {
    //! We are not sending any values
    template <template <typename...> class Tuple, template <typename...> class Variant>
    using value_types = Variant<>;

    //! We are not sending any errors
    template <template <typename...> class Variant>
    using error_types = Variant<>;

    //! We just call @ref concore::set_done()
    static constexpr bool sends_done = true;

    //! The operation state used by this sender
    template <typename R>
    struct oper {
        R receiver_;

        friend void tag_invoke(start_t, oper& self) noexcept {
            concore::set_done((R &&) self.receiver_);
        }
    };

    //! The connect CPO that returns an operation state object
    template <typename R>
    friend oper<R> tag_invoke(connect_t, just_done_sender, R&& r) {
        static_assert(receiver<R>, "just_done() cannot connect to non-receiver");
        return {(R &&) r};
    }
};

} // namespace detail

inline namespace v1 {

/**
 * @brief   Returns a sender that completes with set_done().
 *
 * @details
 *
 * This will return a simple sender that, whenever connected to a receiver and started, it will call
 * @ref set_done() on the receiver object.
 *
 * Postconditions:
 *  - returned sender always calls @ref set_done(), when started
 *  - returned sender never calls @ref set_value()
 *  - returned sender never calls @ref set_error()
 *  - the returned sender has `value_types` = `Variant<Tuple<>>`
 *  - the returned sender has `error_types` = `Variant<>`
 *  - the returned sender does not advertise its completion scheduler
 *
 * @see just(), just_error(), set_done()
 */
inline detail::just_done_sender just_done() noexcept { return {}; }

} // namespace v1
} // namespace concore
