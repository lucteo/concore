#pragma once

#include <concore/_concepts/_concepts_sender.hpp>
#include <concore/_concepts/_concepts_receiver.hpp>
#include <concore/_cpo/_cpo_connect.hpp>

namespace concore {

inline namespace v1 {

#if CONCORE_CXX_HAS_CONCEPTS

// clang-format off
template <typename S, typename R>
concept sender_to
    =  sender<S>
    && receiver<R>
    && requires(S&& s, R&& r) {
        concore::connect((S &&) s, (R &&) r);
    };
    // clang-format on

#else

template <typename S, typename R>
CONCORE_CONCEPT_OR_BOOL sender_to =
        sender<S>&& receiver<R>&& detail::cpo_connect::has_connect<S, R>;

#endif

} // namespace v1

} // namespace concore
