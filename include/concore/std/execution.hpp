#pragma once

#include <concore/detail/cxx_features.hpp>

#include <exception>
#include <type_traits>

#include "_cpo_set_value.hpp"
#include "_cpo_set_done.hpp"
#include "_cpo_set_error.hpp"
#include "_cpo_execute.hpp"
#include "_cpo_connect.hpp"
#include "_cpo_start.hpp"
#include "_cpo_submit.hpp"
#include "_cpo_schedule.hpp"
#include "_cpo_bulk_execute.hpp"
#if CONCORE_CXX_HAS_CONCEPTS
#include "_concepts.hpp"
#endif

namespace concore {

namespace std_execution {

inline namespace v1 {

// Exception types:
struct receiver_invocation_error : std::runtime_error, std::nested_exception {
    receiver_invocation_error() noexcept
        : runtime_error("receiver_invocation_error")
        , nested_exception() {}
};

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