#pragma once

#include <concore/detail/concept_macros.hpp>
#include <concore/_concepts/_concepts_receiver.hpp>
#include <concore/_concepts/_concepts_sender.hpp>
#include <concore/_concepts/_concept_operation_state.hpp>

namespace concore {

namespace detail {
namespace cpo_connect {

CONCORE_DEF_REQUIRES(meets_tag_invoke,                                               //
        CONCORE_LIST(typename Tag, typename S, typename R), CONCORE_LIST(Tag, S, R), //
        (requires(S&& s, R&& r) { tag_invoke(Tag{}, (S &&) s, (R &&) r); }),         // concepts
        tag_invoke(Tag{}, CONCORE_DECLVAL(S), CONCORE_DECLVAL(R))                    // pre-concepts
);

template <typename Tag, typename S, typename R>
CONCORE_CONCEPT_OR_BOOL(
        has_tag_invoke) = receiver_partial<R>&& sender<S>&& meets_tag_invoke<Tag, S, R>;

inline const struct connect_t final {
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename S, typename R), (has_tag_invoke<connect_t, S, R>))
    auto operator()(S&& s, R&& r) const                                 //
            noexcept(noexcept(tag_invoke(*this, (S &&) s, (R &&) r))) { //
        using res_type = decltype(tag_invoke(connect_t{}, (S &&) s, (R &&) r));
        static_assert(operation_state<res_type>,
                "Result type of the custom-defined connect operation must "
                "model the operation_state concept");
        return tag_invoke(connect_t{}, (S &&) s, (R &&) r);
    }
} connect{};

CONCORE_DEF_REQUIRES(has_connect,                                               //
        CONCORE_LIST(typename S, typename R), CONCORE_LIST(S, R),               //
        (requires(S&& s, R&& r) { cpo_connect::connect((S &&) s, (R &&) r); }), // concepts
        cpo_connect::connect(CONCORE_DECLVAL(S), CONCORE_DECLVAL(R))            // pre-concepts
);

} // namespace cpo_connect
} // namespace detail

inline namespace v1 {
using detail::cpo_connect::connect;
using detail::cpo_connect::connect_t;

template <typename Sender, typename Receiver>
using connect_result_t = decltype(connect(CONCORE_DECLVAL(Sender), CONCORE_DECLVAL(Receiver)));

} // namespace v1

} // namespace concore
