#pragma once

#include <concore/detail/cxx_features.hpp>

#include <exception>
#include <type_traits>

namespace concore {

namespace std_execution {

inline namespace v1 {

// Exception types:
struct receiver_invocation_error : std::runtime_error, std::nested_exception {
    receiver_invocation_error() noexcept
        : runtime_error("receiver_invocation_error")
        , nested_exception() {}
};

// Invocable archetype

// TODO: see 2.2.2 Invocable archetype
using invocable_archetype = int;

// Customization points:

// TODO: see 2.2.3 Customization points
// inline namespace unspecified {
// inline constexpr unspecified set_value = unspecified;

// inline constexpr unspecified set_done = unspecified;

// inline constexpr unspecified set_error = unspecified;

// inline constexpr unspecified execute = unspecified;

// inline constexpr unspecified submit = unspecified;

// inline constexpr unspecified schedule = unspecified;

// inline constexpr unspecified bulk_execute = unspecified;
// } // namespace unspecified

// Concepts:

#if CONCORE_CXX_HAS_CONCEPTS

// TODO: see 2.2.4 Concept receiver
template <class T, class E = std::exception_ptr>
concept receiver = true; // std::move_constructible<std::remove_cvref_t<R>> &&
                         // constructible_from<remove_cvref_t<R>, R>; // TODO

// TODO: see 2.2.5 Concept receiver_of
template <class T, class... An>
concept receiver_of = true;

// TODO: see 2.2.6 Concepts sender and sender_to
template <class S>
concept sender = true;

// TODO: see 2.2.7 Concept typed_sender
template <class S>
concept typed_sender = true;

// TODO: see 2.2.6 Concepts sender and sender_to
template <class S, class R>
concept sender_to = true;

// TODO: see 2.2.8 Concept scheduler
template <class S>
concept scheduler = true;

// TODO: see 2.2.9 Concepts executor and executor_of
template <class E>
concept executor = true;

// TODO: see 2.2.9 Concepts executor and executor_of
template <class E, class F>
concept executor_of = true;

#endif

// Sender and receiver utilities type
// TODO: see 2.2.10.1 Class sink_receiver
class sink_receiver;

// TODO: see 2.2.10.2 Class template sender_traits
template <class S>
struct sender_traits;

// Executor type traits:

// TODO: see 2.3.1 Associated shape type
template <class Executor>
struct executor_shape;
// TODO: see 2.3.2 Associated index type
template <class Executor>
struct executor_index;

template <class Executor>
using executor_shape_t = typename executor_shape<Executor>::type;
template <class Executor>
using executor_index_t = typename executor_index<Executor>::type;

// Polymorphic executor support:

// TODO: see 2.4.1 Class bad_executor
class bad_executor;

// TODO: see 2.2.11.2 Polymorphic executor wrappers
template <class... SupportableProperties>
class any_executor;

// TODO: see 2.4.2 Struct prefer_only
template <class Property>
struct prefer_only;
} // namespace v1

} // namespace std_execution

} // namespace concore