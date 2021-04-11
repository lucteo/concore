#pragma once

#include <concore/_concepts/_concepts_sender.hpp>

#include <tuple>

namespace concore {
namespace detail {

/**
 * @brief Helper class that defines the types needed in a sender
 *
 * @tparam SendsDone = false Indicates whether this sender can send the "done" signal
 *
 * @details
 *
 * Used to simplify the definition of senders.
 */
template <bool SendsDone, typename... Values>
struct sender_types_base {
    //! The value types that defines the values that this sender sends to receivers
    template <template <typename...> class Tuple, template <typename...> class Variant>
    using value_types = Variant<Tuple<Values...>>;

    //! The type of error that this sender sends to receiver
    template <template <typename...> class Variant>
    using error_types = Variant<std::exception_ptr>;

    //! Indicates whether this sender can send "done" signal
    static constexpr bool sends_done = SendsDone;
};

//! Query structure to be applied at Variant level, to get the values out of a sender
template <typename... Ts>
struct variant_query_one {};
template <typename T>
struct variant_query_one<T> {
    using type = T;
};
template <>
struct variant_query_one<> {
    struct void_wrapper {
        using type = void;
    };
    using type = void_wrapper;
};

//! Query structure to be applied at Tuple level, to get the values out of a sender
template <typename... Ts>
struct tuple_query_one {
    using type = std::tuple<Ts...>;
};
template <typename T>
struct tuple_query_one<T> {
    using type = T;
};
template <>
struct tuple_query_one<> {
    using type = void;
};

//! Query structure to be applied at Tuple level, to get the transformed values out of a sender
template <typename F>
struct tuple_query_transform {
    template <typename... Ts>
    struct tmptl {
#if CONCORE_CPP_VERSION < 17
        using type = std::result_of<F && (Ts && ...)>::type;
#else
        using type = std::invoke_result_t<F, Ts...>;
#endif
    };
};

//! Get the values_types declared in the sender
template <typename Sender, template <typename...> class Tuple, template <typename...> class Variant>
using sender_value_types = typename sender_traits<Sender>::template value_types<Tuple, Variant>;

//! Get the return type of a sender, assuming the sender returns only one type
template <typename Sender>
using sender_single_return_type =
        typename sender_value_types<Sender, tuple_query_one, variant_query_one>::type::type;

//! Return type for a transform-like operation
template <typename Sender, typename F>
using transform_return_type = typename sender_value_types<Sender,
        tuple_query_transform<F>::template tmptl, variant_query_one>::type::type;


} // namespace detail
} // namespace concore