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
CONCORE_DEF_REQUIRES(meets_inner_fun,                             //
        CONCORE_LIST(typename S, typename R), CONCORE_LIST(S, R), //
        (requires(S&& s, R&& r) { s.connect((R &&) r); }),        // concepts
        CONCORE_DECLVAL(S).connect(CONCORE_DECLVAL(R))            // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_outer_fun,                              //
        CONCORE_LIST(typename S, typename R), CONCORE_LIST(S, R),  //
        (requires(S&& s, R&& r) { connect((S &&) s, (R &&) r); }), // concepts
        connect(CONCORE_DECLVAL(S), CONCORE_DECLVAL(R))            // pre-concepts
);

template <typename Tag, typename S, typename R>
CONCORE_CONCEPT_OR_BOOL(
        has_tag_invoke) = receiver_partial<R>&& sender<S>&& meets_tag_invoke<Tag, S, R>;

template <typename Tag, typename S, typename R>
CONCORE_CONCEPT_OR_BOOL(has_inner_fun) = receiver_partial<R>&& sender<S> &&
                                         !has_tag_invoke<Tag, S, R> && meets_inner_fun<S, R>;

template <typename Tag, typename S, typename R>
CONCORE_CONCEPT_OR_BOOL(has_outer_fun) = receiver_partial<R>&& sender<S> &&
                                         !has_tag_invoke<Tag, S, R> &&
                                         !has_inner_fun<Tag, S, R> && meets_outer_fun<S, R>;

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
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename S, typename R), (has_inner_fun<connect_t, S, R>))
    auto operator()(S&& s, R&& r) const                        //
            noexcept(noexcept(((S &&) s).connect((R &&) r))) { //
        using res_type = decltype(((S &&) s).connect((R &&) r));
        static_assert(operation_state<res_type>,
                "Result type of the custom-defined connect operation must "
                "model the operation_state concept");
        return ((S &&) s).connect((R &&) r);
    }
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename S, typename R), (has_outer_fun<connect_t, S, R>))
    auto operator()(S&& s, R&& r) const                       //
            noexcept(noexcept(connect((S &&) s, (R &&) r))) { //
        using res_type = decltype(connect((S &&) s, (R &&) r));
        static_assert(operation_state<res_type>,
                "Result type of the custom-defined connect operation must "
                "model the operation_state concept");
        return connect((S &&) s, (R &&) r);
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
/**
 * @brief   Connect a sender with a receiver, returning an async operation object
 *
 * @param   snd The sender object, that triggers the work
 * @param   rcv The receiver object that receives the results of the work
 *
 * The type of the `rcv` parameter must model the `receiver` concept.
 *
 * Usage example:
 *      auto op = connect(snd, rcv);
 *      // later
 *      start(op);
 */
using detail::cpo_connect::connect;

/**
 * @brief   Customization-point-object tag for connect
 *
 * To add support for connect to a type S, with the receiver R, one can define:
 *      template <typename R>
 *      void tag_invoke(connect_t, S, R);
 */
using detail::cpo_connect::connect_t;

} // namespace v1

} // namespace concore
