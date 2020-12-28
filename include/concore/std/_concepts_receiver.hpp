#pragma once

#include <concore/detail/concept_macros.hpp>
#include <concore/detail/extra_type_traits.hpp>

#include "_cpo_set_value.hpp"
#include "_cpo_set_done.hpp"
#include "_cpo_set_error.hpp"

#if CONCORE_CXX_HAS_CONCEPTS
#include <concepts>
#endif
#include <exception>
#include <utility>

namespace concore {

namespace std_execution {

#if !CONCORE_CXX_HAS_CONCEPTS
namespace detail {

using cpo_set_done::has_set_done;
using cpo_set_error::has_set_error;
using cpo_set_value::has_set_value;

} // namespace detail
#endif

inline namespace v1 {

#if CONCORE_CXX_HAS_CONCEPTS

template <typename T>
concept receiver_partial = std::move_constructible<std::remove_cvref_t<T>>&&
        std::constructible_from<std::remove_cvref_t<T>, T>&& requires(std::remove_cvref_t<T>&& t) {
    { concore::std_execution::set_done(std::move(t)) }
    noexcept;
};

/**
 * @brief Concept that defines a bare-bone receiver
 *
 * @tparam T The type being checked to see if it's a bare-bone receiver
 * @tparam E The type of errors that the receiver accepts; default std::exception_ptr
 *
 * A receiver is called by a sender when it is done (one way or another) executing work. The
 * receiver is notified for the following cases:
 *  - work is successfully done; set_value() CPO is called
 *  - work is being canceled; set_done() CPO is called
 *  - error occurred during the work; set_error() CPO is called
 *
 * A bare-bone receiver is a receiver that only checks for the following CPOs:
 *  - set_done()
 *  - set_error(E)
 *
 * The set_value() CPO is ignored in a bare-bone receiver, as a receiver may have many ways to be
 * notified about the success of a sender.
 *
 * In addition to these, the type should be move constructible and copy constructible.
 *
 * @see receiver_of, set_done(), set_error()
 */
template <typename T, typename E = std::exception_ptr>
concept receiver = std::move_constructible<std::remove_cvref_t<T>>&&
        std::constructible_from<std::remove_cvref_t<T>, T>&& requires(
                std::remove_cvref_t<T>&& t, E&& e) {
    { concore::std_execution::set_done(std::move(t)) }
    noexcept;
    { concore::std_execution::set_error(std::move(t), (E &&) e) }
    noexcept;
};

/**
 * @brief Concept that defines a receiver of a particular kind
 *
 * @tparam T     The type being checked to see if it's a bare-bone receiver
 * @tparam Vs... The types of the values accepted by the receiver
 * @tparam E     The type of errors that the receiver accepts; default std::exception_ptr
 *
 * A receiver is called by a sender when it is done (one way or another) executing work. The
 * receiver is notified for the following cases:
 *  - work is successfully done; set_value() CPO is called
 *  - work is being canceled; set_done() CPO is called
 *  - error occurred during the work; set_error() CPO is called
 *
 * This concept checks that all three CPOs are defined.
 *
 * This is an extension of the `receiver` concept, but also requiring the set_value() CPO to be
 * present, for a given set of value types.
 *
 * @see receiver, set_value(), set_done(), set_error()
 */
template <typename T, typename E = std::exception_ptr, typename... Vs>
concept receiver_of = receiver<T, E>&& std::move_constructible<std::remove_cvref_t<T>>&&
        std::constructible_from<std::remove_cvref_t<T>, T>&& requires(
                std::remove_cvref_t<T>&& t, Vs&&... vs) {
    concore::std_execution::set_value(std::move(t), (Vs &&) vs...);
};

#else

template <typename T>
CONCORE_CONCEPT_OR_BOOL(
        receiver_partial) = std::is_move_constructible<concore::detail::remove_cvref_t<T>>::
        value&& std::is_constructible<concore::detail::remove_cvref_t<T>,
                T>::value&& detail::has_set_done<concore::detail::remove_cvref_t<T>>;

template <typename T, typename E = std::exception_ptr>
CONCORE_CONCEPT_OR_BOOL(receiver) = std::is_move_constructible<concore::detail::remove_cvref_t<T>>::
        value&& std::is_constructible<concore::detail::remove_cvref_t<T>,
                T>::value&& detail::has_set_done<concore::detail::remove_cvref_t<T>>&& detail::
                has_set_error<concore::detail::remove_cvref_t<T>, E>;

template <typename T, typename... Vs>
CONCORE_CONCEPT_OR_BOOL(receiver_of) =
        receiver<T>&& detail::has_set_value<concore::detail::remove_cvref_t<T>, Vs...>;

#endif

} // namespace v1

} // namespace std_execution
} // namespace concore
