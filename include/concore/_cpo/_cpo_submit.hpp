#pragma once

#include <concore/_concepts/_concept_sender_to.hpp>

namespace concore {

namespace detail {
namespace cpo_submit {

CONCORE_DEF_REQUIRES(meets_tag_invoke,                                               //
        CONCORE_LIST(typename Tag, typename S, typename R), CONCORE_LIST(Tag, S, R), //
        (requires(S&& s, R&& r) { tag_invoke(Tag{}, (S &&) s, (R &&) r); }),         // concepts
        tag_invoke(Tag{}, CONCORE_DECLVAL(S), CONCORE_DECLVAL(R))                    // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_inner_fun,                             //
        CONCORE_LIST(typename S, typename R), CONCORE_LIST(S, R), //
        (requires(S&& s, R&& r) { s.submit((R &&) r); }),         // concepts
        CONCORE_DECLVAL(S).submit(CONCORE_DECLVAL(R))             // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_outer_fun,                             //
        CONCORE_LIST(typename S, typename R), CONCORE_LIST(S, R), //
        (requires(S&& s, R&& r) { submit((S &&) s, (R &&) r); }), // concepts
        submit(CONCORE_DECLVAL(S), CONCORE_DECLVAL(R))            // pre-concepts
);

template <typename Tag, typename S, typename R>
CONCORE_CONCEPT_OR_BOOL(has_tag_invoke) = sender_to<S, R>&& meets_tag_invoke<Tag, S, R>;

template <typename Tag, typename S, typename R>
CONCORE_CONCEPT_OR_BOOL(has_inner_fun) = sender_to<S, R> &&
                                         !has_tag_invoke<Tag, S, R> && meets_inner_fun<S, R>;

template <typename Tag, typename S, typename R>
CONCORE_CONCEPT_OR_BOOL(has_outer_fun) = sender_to<S, R> && !has_tag_invoke<Tag, S, R> &&
                                         !has_inner_fun<Tag, S, R> && meets_outer_fun<S, R>;

template <typename Tag, typename S, typename R>
CONCORE_CONCEPT_OR_BOOL(has_fallback_fun) = sender_to<S, R> && !has_tag_invoke<Tag, S, R> &&
                                            !has_inner_fun<Tag, S, R> && !meets_outer_fun<S, R>;

inline const struct submit_t final {
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename S, typename R), (has_tag_invoke<submit_t, S, R>))
    void operator()(S&& s, R&& r) const                                 //
            noexcept(noexcept(tag_invoke(*this, (S &&) s, (R &&) r))) { //
        tag_invoke(submit_t{}, (S &&) s, (R &&) r);
    }
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename S, typename R), (has_inner_fun<submit_t, S, R>))
    void operator()(S&& s, R&& r) const                       //
            noexcept(noexcept(((S &&) s).submit((R &&) r))) { //
        ((S &&) s).submit((R &&) r);
    }
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename S, typename R), (has_outer_fun<submit_t, S, R>))
    void operator()(S&& s, R&& r) const                      //
            noexcept(noexcept(submit((S &&) s, (R &&) r))) { //
        submit((S &&) s, (R &&) r);
    }
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename S, typename R), (has_fallback_fun<submit_t, S, R>))
    void operator()(S&& s, R&& r) const {
        // Fallback to connect
        auto op = concore::connect((S &&) s, (R &&) r);
        concore::start(op);
    }
} submit{};

} // namespace cpo_submit
} // namespace detail

inline namespace v1 {
/**
 * @brief   Submit work from a sender, by combining it with a receiver
 *
 * @param   snd The sender object, that triggers the work
 * @param   rcv The receiver object that receives the results of the work
 *
 * For S and R the types of the two parameters, the expression `sender_to<S, R>` must be true.
 *
 * Usage example:
 *      submit(snd, rcv);
 */
using detail::cpo_submit::submit;

/**
 * @brief   Customization-point-object tag for submit
 *
 * To add support for submit to a type S, with the receiver R, one can define:
 *      template <typename R>
 *      void tag_invoke(submit_t, S, R);
 */
using detail::cpo_submit::submit_t;

} // namespace v1

} // namespace concore
