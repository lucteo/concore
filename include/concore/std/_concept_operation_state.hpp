#pragma once

#include <concore/detail/concept_macros.hpp>
#include <concore/detail/extra_type_traits.hpp>

#include "_cpo_start.hpp"

#if CONCORE_CXX_HAS_CONCEPTS
#include <concepts>
#endif

namespace concore {
namespace std_execution {
inline namespace v1 {

#if CONCORE_CXX_HAS_CONCEPTS

/**
 * @brief   Concept that defines an operation state
 *
 * @tparam  OpState The type that is being checked to see if it's a operation_state
 *
 * An operation state is a pack between a sender and a receiver. It contains an associated execution
 * context and the operation that needs to be executed within that context (sender + receiver).
 *
 * A compatible type must implement the start() CPO. In addition, any object of this type must be
 * destructible. Only object types model operation states.
 *
 * @see sender, receiver, connect()
 */
template <typename OpState>
concept operation_state = std::destructible<OpState>&& std::is_object_v<OpState>&& requires(
        OpState& op) {
    { concore::std_execution::start(op) }
    noexcept;
};

#else

template <typename OpState>
CONCORE_CONCEPT_OR_BOOL(operation_state) = std::is_destructible_v<OpState>&& std::is_object_v<
        OpState>&& detail::cpo_start::has_start<concore::detail::remove_cvref_t<OpState>>;

#endif

} // namespace v1
} // namespace std_execution
} // namespace concore
