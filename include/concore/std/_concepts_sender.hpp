#pragma once

#include <concore/detail/concept_macros.hpp>
#include <concore/detail/extra_type_traits.hpp>

#include "_cpo_set_value.hpp"
#include "_cpo_set_done.hpp"
#include "_cpo_set_error.hpp"

#if CONCORE_CXX_HAS_CONCEPTS
#include <concepts>
#endif

namespace concore {

namespace std_execution {

namespace detail {

template <template <template <class...> class Tuple, template <class...> class Variant> class>
struct has_value_types;

template <template <template <class...> class> class Variant>
struct has_error_types;

#if CONCORE_CXX_HAS_CONCEPTS

template <typename S>
concept has_sender_types = requires() {
    typename std::integral_constant<bool, S::sends_done>;
    typename has_value_types<S::template value_types>;
    typename has_error_types<S::template error_types>;
};

#else

double has_sender_types_tester(...);
template <typename S, typename = std::integral_constant<bool, S::sends_done>,
        typename = has_value_types<S::template value_types>,
        typename = has_error_types<S::template error_types>>
char has_sender_types_tester(S*);

template <typename S>
inline constexpr bool has_sender_types = sizeof(has_sender_types_tester(
                                                 static_cast<S*>(nullptr))) == 1;
#endif

template <typename S>
struct typed_sender_traits {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = typename S::template value_types<Tuple, Variant>;

    template <template <class...> class Variant>
    using error_types = typename S::template error_types<Variant>;

    static constexpr bool sends_done = S::sends_done;
};

template <typename S>
struct no_sender_traits {
    using __unspecialized = void;
};

template <typename S>
using sender_traits_impl =
        std::conditional<has_sender_types<S>, typed_sender_traits<S>, no_sender_traits<S>>;

template <typename S>
CONCORE_CONCEPT_OR_BOOL(is_sender) = has_sender_types<S>;

} // namespace detail

inline namespace v1 {

template <typename S>
struct sender_traits : detail::sender_traits_impl<S>::type {};

#if CONCORE_CXX_HAS_CONCEPTS

/**
 * @brief   Concept that defines a sender
 *
 * @tparam  S   The type that is being checked to see if it's a sender
 *
 * A sender is is an object that does some work and then invokes a receiver when the work is done
 * (or when the work is being canceled, or an error occurred).
 *
 * The sender, after starts executing work, needs to exit in one of the three ways:
 *  - work is successfully done; it calls set_value() on the receiver
 *  - work is canceled; it calls set_done() on the receiver
 *  - work yields an error; it calls set_erro() on the receiver
 *
 * The sender starts working when submit(S, R) or start(connect(S, R)) is called passing the sender
 * object in. The sender should not execute any other work after calling one of the three exit
 * functions. The sender should not finish its work without calling one of these three exit
 * functions.
 *
 * A sender typically has a connect() method to connect to a receiver, but this is not mandatory. A
 * CPO can be provided instead for the connect() method.
 *
 * A sender typically exposes the type of the values it sets, and the type of errors it can
 * generate, but this is not mandatory.
 *
 * @see typed_sender, sender_to
 */
template <typename S>
concept sender =
        std::move_constructible<std::remove_cvref_t<S>>&& detail::is_sender<std::remove_cvref_t<S>>;

/**
 * @brief   Concept that defines a typed sender
 *
 * @tparam  S   The type that is being checked to see if it's a typed sender
 *
 * This is just like the @ref sender concept, but it requres the type information; that is the types
 * that expose the types of values it sets to the receiver and the type of errors it can generate.
 *
 * @see sender, sender_to
 */
template <typename S>
concept typed_sender = sender<S>&& detail::has_sender_types<std::remove_cvref_t<S>>;

#else

template <typename S>
CONCORE_CONCEPT_OR_BOOL(sender) =
        std::is_move_constructible<concore::detail::remove_cvref_t<S>>::value&& detail::is_sender<
                concore::detail::remove_cvref_t<S>>;

template <typename S>
CONCORE_CONCEPT_OR_BOOL(
        typed_sender) = std::is_move_constructible<concore::detail::remove_cvref_t<S>>::
        value&& detail::has_sender_types<concore::detail::remove_cvref_t<S>>;

#endif

} // namespace v1

} // namespace std_execution
} // namespace concore
