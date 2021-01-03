#pragma once

#include <concore/detail/concept_macros.hpp>

namespace concore {

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

using detail::cpo_set_value::set_value;
using detail::cpo_set_value::set_value_t;

} // namespace v1
} // namespace concore