/**
 * @file    just.hpp
 * @brief   Definition of @ref concore::v1::just "just"
 *
 * @see     @ref concore::v1::just "just"
 */
#pragma once

#include <concore/detail/extra_type_traits.hpp>
#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <concore/_cpo/_cpo_connect.hpp>
#include <concore/_cpo/_cpo_start.hpp>
#include <concore/_concepts/_concepts_receiver.hpp>

#include <tuple>
#include <exception>

namespace concore {

namespace detail {

template <CONCORE_CONCEPT_OR_TYPENAME(CONCORE_CONCEPT_OR_TYPENAME(detail::moveable_value))... Ts>
struct just_sender {
    //! We can send to receivers the value type(s) received as template args
    template <template <typename...> class Tuple, template <typename...> class Variant>
    using value_types = Variant<Tuple<Ts...>>;

    //! Our only error type is exception_ptr
    template <template <typename...> class Variant>
    using error_types = Variant<std::exception_ptr>;

    //! Cannot call @ref concore::set_done()
    static constexpr bool sends_done = false;

    just_sender(Ts... vals)
        : values_((Ts &&) vals...) {}

    //! The operation state used by this sender
    template <typename R>
    struct oper {
        R receiver_;
        std::tuple<Ts...> values_;

        friend void tag_invoke(start_t, oper& self) noexcept {
            try {
                auto set_value_wrapper = [&](Ts&&... vals) {
                    concore::set_value((R &&) self.receiver_, (Ts &&) vals...);
                };
                std::apply(std::move(set_value_wrapper), std::move(self.values_));
            } catch (...) {
                concore::set_error((R &&) self.receiver_, std::current_exception());
            }
        }
    };

    //! The connect CPO that returns an operation state object
    template <typename R>
    friend oper<R> tag_invoke(connect_t, just_sender&& s, R&& r) {
        static_assert(receiver_of<R, Ts...>, "just() cannot connect to a non-matching receiver");
        return {(R &&) r, std::move(s.values_)};
    }
    //! @overload
    template <typename R>
    friend oper<R> tag_invoke(connect_t, const just_sender& s, R&& r) {
        static_assert(receiver_of<R, Ts...>, "just() cannot connect to a non-matching receiver");
        return {(R &&) r, s.values_};
    }

private:
    std::tuple<Ts...> values_;
};

} // namespace detail

inline namespace v1 {

/**
 * @brief   Returns a sender that completes with the given set of values.
 *
 * @tparam Ts   The types of values to be sent
 *
 * @param values The values sent by the returned sender
 *
 * @details
 *
 * This will return a simple sender that, whenever connected to a receiver and started, it will call
 * @ref set_value() on the receiver object, passing the received values.
 *
 * Preconditions:
 *  - Ts are movable
 *
 * Postconditions:
 *  - returned sender always calls @ref set_value() passing the given values, when started
 *  - if calling @ref set_value() throws, this will subsequently call @ref set_error() with the
 * caught exception
 *  - returned sender never calls @ref set_done()
 *  - the returned sender has `value_types` = `Variant<Tuple<Ts>>`
 *  - the returned sender has `error_types` = `Variant<std::exception_ptr>`
 *  - the returned sender does not advertise its completion scheduler
 *
 * @see just_error(), just_done(), set_value()
 */
template <CONCORE_CONCEPT_OR_TYPENAME(detail::moveable_value)... Ts>
inline detail::just_sender<Ts...> just(Ts&&... values) noexcept(
        std::is_nothrow_constructible_v<std::tuple<Ts...>, Ts...>) {
    return detail::just_sender<Ts...>{(Ts &&) values...};
}

} // namespace v1
} // namespace concore
