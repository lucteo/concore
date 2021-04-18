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

template <typename F>
struct tuple_query_transform_ref {
    template <typename... Ts>
    struct tmptl {
#if CONCORE_CPP_VERSION < 17
        using type = std::result_of<F && (Ts & ...)>::type;
#else
        using type = std::invoke_result_t<F, Ts&...>;
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

//! Return type for a transform-like operation, passing the input as reference
template <typename Sender, typename F>
using transform_ref_return_type = typename sender_value_types<Sender,
        tuple_query_transform_ref<F>::template tmptl, variant_query_one>::type::type;

template <typename F>
struct sender_algo_wrapper;

template <typename F1, typename F2>
struct concat_sender_algo_wrapper_fun {
    sender_algo_wrapper<F1> w1_;
    sender_algo_wrapper<F2> w2_;

    template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender>
    auto operator()(Sender&& sender) const& {
        return w2_(w1_(sender));
    }

    template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender>
    auto operator()(Sender&& sender) && {
        return std::move(w2_)(std::move(w1_)(sender));
    }
};

//! Wrapper for a sender algorithm. This allows the input sender to be passed in later to a sender
//! algorithm. It allows the sender algorithm to be piped in as p1897 requires.
template <typename F>
struct sender_algo_wrapper {
    //! The functor used to create the actual sender instance
    F f_;

    explicit sender_algo_wrapper(F&& f)
        : f_((F &&) f) {}

    //! Call operator that will create the final sender given the input sender
    template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender>
    auto operator()(Sender&& sender) const& {
        return f_((Sender &&) sender);
    }
    //! Call operator that will create the final sender given the input sender -- move version
    template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender>
    auto operator()(Sender&& sender) && {
        return ((F &&) f_)((Sender &&) sender);
    }

    //! Pipe operator, allowing us to pipe the sender algorithms
    template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender>
    friend auto operator|(Sender&& sender, sender_algo_wrapper&& self) {
        return std::move(self.f_)((Sender &&) sender);
    }
    template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender>
    friend auto operator|(Sender&& sender, const sender_algo_wrapper& self) {
        return self.f_((Sender &&) sender);
    }

    //! Pipe operator, allowing us to concatenate two wrappers
    template <typename F1>
    friend auto operator|(sender_algo_wrapper<F1>&& w1, sender_algo_wrapper&& self) {
        return make_sender_algo_wrapper(
                concat_sender_algo_wrapper_fun<F1, F>{std::move(w1), std::move(self)});
    }
    template <typename F1>
    friend auto operator|(const sender_algo_wrapper<F1>& w1, const sender_algo_wrapper& self) {
        return make_sender_algo_wrapper(concat_sender_algo_wrapper_fun<F1, F>{w1, self});
    }
};

template <typename F>
inline auto make_sender_algo_wrapper(F&& f) {
    return sender_algo_wrapper<F>((F &&) f);
}

} // namespace detail
} // namespace concore