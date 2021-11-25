#pragma once

#include <concore/detail/concept_macros.hpp>
#include <concore/detail/extra_type_traits.hpp>

#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_done.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>

#if CONCORE_CXX_HAS_CONCEPTS
#include <concepts>
#endif

namespace concore {

namespace detail {

struct sender_base {};

template <template <template <class...> class Tuple, template <class...> class Variant> class>
struct has_value_types;

template <template <template <class...> class> class Variant>
struct has_error_types;

template <typename... Ts>
struct type_array {};

#if CONCORE_CXX_HAS_CONCEPTS

template <typename S>
concept has_sender_types = requires() {
    typename std::bool_constant<S::sends_done>;
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
struct sender_traits_base {};

template <typename S>
struct no_sender_traits {
    using __unspecialized = void;
};

template <typename S, bool has_types = has_sender_types<S>,
        bool is_derived = std::is_base_of<sender_base, S>::value>
struct sender_traits_impl;

template <typename S, bool is_derived>
struct sender_traits_impl<S, true, is_derived> {
    using type = typed_sender_traits<S>;
};
template <typename S>
struct sender_traits_impl<S, false, true> {
    using type = sender_traits_base<S>;
};
template <typename S>
struct sender_traits_impl<S, false, false> {
    using type = no_sender_traits<S>;
};

template <typename S>
CONCORE_CONCEPT_OR_BOOL is_sender = has_sender_types<S> || std::is_base_of<sender_base, S>::value;

template <typename S, bool typed = has_sender_types<remove_cvref_t<S>>>
struct sender_has_values_impl {
    template <typename... Vs>
    using match = std::is_same<type_array<Vs...>,
            typename typed_sender_traits<S>::template value_types<type_array, type_identity>>;
};
template <typename S>
struct sender_has_values_impl<S, false> {
    template <typename... Vs>
    using match = std::false_type;
};

} // namespace detail

inline namespace v1 {

using detail::sender_base;

template <typename S>
struct sender_traits : detail::sender_traits_impl<S>::type {};

template <typename S>
CONCORE_CONCEPT_OR_BOOL sender =                                                //
        (std::is_move_constructible<concore::detail::remove_cvref_t<S>>::value) //
        && detail::is_sender<concore::detail::remove_cvref_t<S>>;

template <typename S>
CONCORE_CONCEPT_OR_BOOL typed_sender = //
        (sender<S>)                    //
        &&detail::has_sender_types<concore::detail::remove_cvref_t<S>>;

template <typename S, typename... Vs>
CONCORE_CONCEPT_OR_BOOL sender_of = //
        typed_sender<S>             //
                && detail::sender_has_values_impl<S>::template match<Vs...>::value;

} // namespace v1

} // namespace concore
