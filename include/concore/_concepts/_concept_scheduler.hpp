#pragma once

#include <concore/detail/cxx_features.hpp>

#if CONCORE_CXX_HAS_CONCEPTS

#include <concore/_cpo/_cpo_schedule.hpp>

#include <concepts>
#include <type_traits>

namespace concore {

namespace detail {}; // namespace detail

inline namespace v1 {

/**
 * @brief   Concept that defines a scheduler
 *
 * @tparam  S   The type that is being checked to see if it's a scheduler
 *
 * A scheduler type allows a schedule() operation that creates a sender out of the scheduler. A
 * typical scheduler contains an execution context that will pass to the sender on its creation.
 *
 * The type that match this concept must be move and copy constructible and must also define the
 * schedule() CPO.
 *
 * @see sender
 */
template <typename S>
concept scheduler = std::move_constructible<std::remove_cvref_t<S>>&&
        std::equality_comparable<std::remove_cvref_t<S>>&& requires(S&& s) {
    concore::schedule((S &&) s);
};

} // namespace v1

} // namespace concore

#endif