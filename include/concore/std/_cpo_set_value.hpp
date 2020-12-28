#pragma once

#include <concore/detail/concept_macros.hpp>

namespace concore {
namespace std_execution {

namespace detail {
namespace cpo_set_value {

CONCORE_DEF_REQUIRES(meets_tag_invoke,                                                       //
        CONCORE_LIST(typename Tag, typename R, typename... Vs), CONCORE_LIST(Tag, R, Vs...), //
        (requires(R&& r, Vs&&... vs) { tag_invoke(Tag{}, (R &&) r, (Vs &&) vs...); }), // concepts
        tag_invoke(Tag{}, CONCORE_DECLVAL(R), CONCORE_DECLVAL(Vs)...) // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_inner_fun,                                     //
        CONCORE_LIST(typename R, typename... Vs), CONCORE_LIST(R, Vs...), //
        (requires(R&& r, Vs&&... vs) { r.set_value((Vs &&) vs...); }),    // concepts
        CONCORE_DECLVAL(R).set_value(CONCORE_DECLVAL(Vs)...)              // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_outer_fun,                                          //
        CONCORE_LIST(typename R, typename... Vs), CONCORE_LIST(R, Vs...),      //
        (requires(R&& r, Vs&&... vs) { set_value((R &&) r, (Vs &&) vs...); }), // concepts
        set_value(CONCORE_DECLVAL(R), CONCORE_DECLVAL(Vs)...)                  // pre-concepts
);

template <typename Tag, typename R, typename... Vs>
CONCORE_CONCEPT_OR_BOOL(has_tag_invoke) = meets_tag_invoke<Tag, R, Vs...>;

template <typename Tag, typename R, typename... Vs>
CONCORE_CONCEPT_OR_BOOL(
        has_inner_fun) = !has_tag_invoke<Tag, R, Vs...> && meets_inner_fun<R, Vs...>;

template <typename Tag, typename R, typename... Vs>
CONCORE_CONCEPT_OR_BOOL(has_outer_fun) = !has_tag_invoke<Tag, R, Vs...> &&
                                         !has_inner_fun<Tag, R, Vs...> && meets_outer_fun<R, Vs...>;

inline const struct set_value_t final {
    CONCORE_TEMPLATE_COND(
            CONCORE_LIST(typename R, typename... Vs), (has_tag_invoke<set_value_t, R, Vs...>))
    void operator()(R&& r, Vs&&... vs) const                                 //
            noexcept(noexcept(tag_invoke(*this, (R &&) r, (Vs &&) vs...))) { //
        tag_invoke(set_value_t{}, (R &&) r, (Vs &&) vs...);
    }
    CONCORE_TEMPLATE_COND(
            CONCORE_LIST(typename R, typename... Vs), (has_inner_fun<set_value_t, R, Vs...>))
    void operator()(R&& r, Vs&&... vs) const                          //
            noexcept(noexcept(((R &&) r).set_value((Vs &&) vs...))) { //
        ((R &&) r).set_value((Vs &&) vs...);
    }
    CONCORE_TEMPLATE_COND(
            CONCORE_LIST(typename R, typename... Vs), (has_outer_fun<set_value_t, R, Vs...>))
    void operator()(R&& r, Vs&&... vs) const                         //
            noexcept(noexcept(set_value((R &&) r, (Vs &&) vs...))) { //
        set_value((R &&) r, (Vs &&) vs...);
    }
} set_value{};

template <typename R, typename... Vs>
CONCORE_CONCEPT_OR_BOOL(has_set_value) = meets_tag_invoke<set_value_t, R, Vs...> ||
                                         meets_inner_fun<R, Vs...> || meets_outer_fun<R, Vs...>;

} // namespace cpo_set_value
} // namespace detail

inline namespace v1 {

/**
 * @brief   Function-like object that can be used to set values in receivers
 *
 * @tparam  R       The type of receiver we want to use; must model `receiver_of<Vs...>`
 * @tparam  Vs...   The type of value(s) to be set to the receiver
 *
 * @param   r       The receiver object that is signaled about sender's success
 * @param   vs...   The values sent by the sender
 *
 * This is called by a sender whenever the sender has finished work and produces some values. This
 * can be called even if the sender doesn't have any values to send to the receiver.
 *
 * `receiver_of<R, Vs...>` must be true.
 */
using detail::cpo_set_value::set_value;

/**
 * @brief   Type to use for customization point for set_value
 *
 * This can be used for types that do not directly model the receiver concepts. One can define a
 * `tag_invoke` customization point to make the type be a receiver.
 *
 * For a type to be receiver, it needs to have the following customization points:
 *  - `tag_invoke(set_value_t, receiver, ...)`
 *  - `tag_invoke(set_done_t, receiver)`
 *  - `tag_invoke(set_error_t, receiver, err)`
 *
 * @see     set_value
 */
using detail::cpo_set_value::set_value_t;

} // namespace v1
} // namespace std_execution
} // namespace concore