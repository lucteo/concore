#pragma once

#include <concore/detail/concept_macros.hpp>

namespace concore {

namespace detail {
namespace cpo_set_error {

CONCORE_DEF_REQUIRES(meets_tag_invoke,                                               //
        CONCORE_LIST(typename Tag, typename R, typename E), CONCORE_LIST(Tag, R, E), //
        (requires(R&& r, E&& e) {
            { tag_invoke(Tag{}, (R &&) r, (E &&) e) }
            noexcept;
        }),                                                       // concepts
        tag_invoke(Tag{}, CONCORE_DECLVAL(R), CONCORE_DECLVAL(E)) // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_inner_fun,                             //
        CONCORE_LIST(typename R, typename E), CONCORE_LIST(R, E), //
        (requires(R&& r, E&& e) {
            { r.set_error((E &&) e) }
            noexcept;
        }),                                              // concepts
        CONCORE_DECLVAL(R).set_error(CONCORE_DECLVAL(E)) // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_outer_fun,                             //
        CONCORE_LIST(typename R, typename E), CONCORE_LIST(R, E), //
        (requires(R&& r, E&& e) {
            { set_error((R &&) r, (E &&) e) }
            noexcept;
        }),                                               // concepts
        set_error(CONCORE_DECLVAL(R), CONCORE_DECLVAL(E)) // pre-concepts
);

template <typename Tag, typename R, typename E>
CONCORE_CONCEPT_OR_BOOL(has_tag_invoke) = meets_tag_invoke<Tag, R, E>;

template <typename Tag, typename R, typename E>
CONCORE_CONCEPT_OR_BOOL(has_inner_fun) = !has_tag_invoke<Tag, R, E> && meets_inner_fun<R, E>;

template <typename Tag, typename R, typename E>
CONCORE_CONCEPT_OR_BOOL(has_outer_fun) = !has_tag_invoke<Tag, R, E> &&
                                         !has_inner_fun<Tag, R, E> && meets_outer_fun<R, E>;

inline const struct set_error_t final {
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename R, typename E), (has_tag_invoke<set_error_t, R, E>))
    void operator()(R&& r, E&& e) const noexcept { //
        tag_invoke(set_error_t{}, (R &&) r, (E &&) e);
    }
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename R, typename E), (has_inner_fun<set_error_t, R, E>))
    void operator()(R&& r, E&& e) const noexcept { //
        ((R &&) r).set_error((E &&) e);
    }
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename R, typename E), (has_outer_fun<set_error_t, R, E>))
    void operator()(R&& r, E&& e) const noexcept { //
        set_error((R &&) r, (E &&) e);
    }
} set_error{};

template <typename R, typename E>
CONCORE_CONCEPT_OR_BOOL(has_set_error) =
        meets_tag_invoke<set_error_t, R, E> || meets_inner_fun<R, E> || meets_outer_fun<R, E>;

} // namespace cpo_set_error
} // namespace detail

inline namespace v1 {

/**
 * @brief   Function-like object that can be used to notify receivers of errors
 *
 * @tparam  R       The type of receiver we want to use; must model `receiver`
 * @tparam  E       The type of error we want to pass to the receiver
 *
 * @param   r       The receiver object that is signaled about sender's error
 * @param   e       The error sent by the sender to the receiver
 *
 * This is called by a sender whenever the sender has an error to report to the sender. Sending an
 * error means that the sender is done processing.
 *
 * `receiver<R, E>` must be true.
 */
using detail::cpo_set_error::set_error;

/**
 * @brief   Type to use for customization point for set_error
 *
 * This can be used for types that do not directly model the receiver concepts. One can define a
 * `tag_invoke` customization point to make the type be a receiver.
 *
 * For a type to be receiver, it needs to have the following customization points:
 *  - `tag_invoke(set_value_t, receiver, ...)`
 *  - `tag_invoke(set_done_t, receiver)`
 *  - `tag_invoke(set_error_t, receiver, err)`
 *
 * @see     set_error
 */
using detail::cpo_set_error::set_error_t;

} // namespace v1
} // namespace concore