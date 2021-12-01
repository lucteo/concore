/**
 * @file    just_error.hpp
 * @brief   Definition of @ref concore::v1::just_error "just_error"
 *
 * @see     @ref concore::v1::just_error "just_error"
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

template <CONCORE_CONCEPT_OR_TYPENAME(CONCORE_CONCEPT_OR_TYPENAME(detail::moveable_value)) T>
struct just_error_sender {
    //! We are not sending any values
    template <template <typename...> class Tuple, template <typename...> class Variant>
    using value_types = Variant<>;

    //! Our error types are T and exception_ptr
    //! If T == exception_ptr, report the type only once
    template <template <typename...> class Variant>
    using error_types = typename std::conditional<        //
            std::is_same<T, std::exception_ptr()>::value, //
            Variant<std::exception_ptr>,                  //
            Variant<T, std::exception_ptr>>::type;

    //! Cannot call @ref concore::set_done()
    static constexpr bool sends_done = false;

    just_error_sender(T err) noexcept
        : err_((T &&) err) {}

    //! The operation state used by this sender
    template <typename R>
    struct oper {
        R receiver_;
        T err_;

        friend void tag_invoke(start_t, oper& self) noexcept {
            concore::set_error((R &&) self.receiver_, std::move(self.err_));
        }
    };

    //! The connect CPO that returns an operation state object
    template <typename R>
    friend oper<R> tag_invoke(connect_t, just_error_sender&& s, R&& r) {
        static_assert(receiver<R, T>, "just_error() cannot connect to a non-matching receiver");
        return {(R &&) r, std::move(s.err_)};
    }
    //! @overload
    template <typename R>
    friend oper<R> tag_invoke(connect_t, const just_error_sender& s, R&& r) {
        static_assert(receiver<R, T>, "just_error() cannot connect to a non-matching receiver");
        return {(R &&) r, s.err_};
    }

private:
    T err_;
};

} // namespace detail

inline namespace v1 {

/**
 * @brief   Returns a sender that completes with the given error.
 *
 * @tparam T    The type of the error to be sent
 *
 * @param err   The error to be sent by the returned sender to the receiver connecting to it
 *
 * @details
 *
 * This will return a simple sender that, whenever connected to a receiver and started, it will call
 * @ref set_error() on the receiver object.
 *
 * Preconditions:
 *  - T is nothrow movable
 *
 * Postconditions:
 *  - returned sender always calls @ref set_error() passing the given object, when started
 *  - returned sender never calls @ref set_value()
 *  - returned sender never calls @ref set_done()
 *  - the returned sender has `value_types` = `Variant<Tuple<>>`
 *  - the returned sender has `error_types` = `Variant<std::exception_ptr>`, if T == `exception_ptr`
 *  - the returned sender has `error_types` = `Variant<T, std::exception_ptr>`, if T !=
 * `exception_ptr`
 *  - the returned sender does not advertise its completion scheduler
 *
 * @see just(), just_done(), set_error()
 */
template <CONCORE_CONCEPT_OR_TYPENAME(detail::moveable_value) T>
CONCORE_REQUIRES_OPT(std::is_nothrow_move_constructible_v<T>)
inline detail::just_error_sender<T> just_error(T&& err) noexcept {
    return {(T &&) err};
}

} // namespace v1
} // namespace concore
