#pragma once

#include <concore/_cpo/_tag_invoke.hpp>
#include <concore/_concepts/_concepts_receiver.hpp>
#include <concore/_concepts/_concepts_sender.hpp>
#include <concore/_concepts/_concept_operation_state.hpp>

namespace concore {

namespace detail {
namespace cpo_connect {

template <typename Tag, typename S, typename R>
CONCORE_CONCEPT_OR_BOOL can_connect = receiver_partial<R>&& sender<S>&& tag_invocable<Tag, S, R>;

inline const struct connect_t final {
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename S, typename R), //
            (can_connect<connect_t, S, R>))
    auto operator()(S&& s, R&& r) const                        //
            noexcept(nothrow_tag_invocable<connect_t, S, R>) { //
        using res_type = decltype(tag_invoke(connect_t{}, (S &&) s, (R &&) r));
        ;
        static_assert(operation_state<res_type>,
                "Result type of the custom-defined connect operation must "
                "model the operation_state concept");

        return tag_invoke(connect_t{}, (S &&) s, (R &&) r);
    }
} connect{};

template <typename S, typename R>
CONCORE_CONCEPT_OR_BOOL has_connect = can_connect<connect_t, S, R>;

} // namespace cpo_connect
} // namespace detail

inline namespace v1 {
using detail::cpo_connect::connect;
using detail::cpo_connect::connect_t;

template <typename Sender, typename Receiver>
using connect_result_t = decltype(connect(CONCORE_DECLVAL(Sender), CONCORE_DECLVAL(Receiver)));

} // namespace v1

} // namespace concore
