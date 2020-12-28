#pragma once

#include "_concepts_sender.hpp"
#include "_concepts_receiver.hpp"
#include "_cpo_connect.hpp"

namespace concore {

namespace std_execution {

inline namespace v1 {

#if CONCORE_CXX_HAS_CONCEPTS

/**
 * @brief   Concept that defines a sender compatible with a given receiver
 *
 * @tparam  S   The type that is being checked to see if it's a sender
 * @tparam  R   The type the receiver that the sender needs to be compatible with
 *
 * This is true if S is models the @ref sender concept, R models the @ref receiver concept, and
 * there is a connect() CPO that can connect the sender and the receiver.
 *
 * @see sender, typed_sender
 */
template <typename S, typename R>
concept sender_to = sender<S>&& receiver_partial<R>&& requires(S&& s, R&& r) {
    concore::std_execution::connect((S &&) s, (R &&) r);
};

#else

template <typename S, typename R>
CONCORE_CONCEPT_OR_BOOL(
        sender_to) = sender<S>&& receiver_partial<R>&& detail::cpo_connect::has_connect<S, R>;

#endif

} // namespace v1

} // namespace std_execution
} // namespace concore
