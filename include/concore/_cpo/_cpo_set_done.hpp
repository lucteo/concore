#pragma once

#include <concore/detail/concept_macros.hpp>

namespace concore {

namespace detail {
namespace cpo_set_done {

CONCORE_DEF_REQUIRES(meets_tag_invoke,                                //
        CONCORE_LIST(typename Tag, typename R), CONCORE_LIST(Tag, R), //
        (requires(R&& r) { tag_invoke(Tag{}, (R &&) r); }),           // concepts
        tag_invoke(Tag{}, CONCORE_DECLVAL(R))                         // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_inner_fun,        //
        typename R, R,                       //
        (requires(R&& r) { r.set_done(); }), // concepts
        CONCORE_DECLVAL(R).set_done()        // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_outer_fun,              //
        typename R, R,                             //
        (requires(R&& r) { set_done((R &&) r); }), // concepts
        set_done(CONCORE_DECLVAL(R))               // pre-concepts
);

template <typename Tag, typename R>
CONCORE_CONCEPT_OR_BOOL(has_tag_invoke) = meets_tag_invoke<Tag, R>;

template <typename Tag, typename R>
CONCORE_CONCEPT_OR_BOOL(has_inner_fun) = !has_tag_invoke<Tag, R> && meets_inner_fun<R>;

template <typename Tag, typename R>
CONCORE_CONCEPT_OR_BOOL(has_outer_fun) = !has_tag_invoke<Tag, R> &&
                                         !has_inner_fun<Tag, R> && meets_outer_fun<R>;

inline const struct set_done_t final {
    CONCORE_TEMPLATE_COND(typename R, (has_tag_invoke<set_done_t, R>))
    void operator()(R&& r) const                              //
            noexcept(noexcept(tag_invoke(*this, (R &&) r))) { //
        tag_invoke(set_done_t{}, (R &&) r);
    }
    CONCORE_TEMPLATE_COND(typename R, (has_inner_fun<set_done_t, R>))
    void operator()(R&& r) const                        //
            noexcept(noexcept(((R &&) r).set_done())) { //
        ((R &&) r).set_done();
    }
    CONCORE_TEMPLATE_COND(typename R, (has_outer_fun<set_done_t, R>))
    void operator()(R&& r) const                     //
            noexcept(noexcept(set_done((R &&) r))) { //
        set_done((R &&) r);
    }
} set_done{};

template <typename R>
CONCORE_CONCEPT_OR_BOOL(
        has_set_done) = meets_tag_invoke<set_done_t, R> || meets_inner_fun<R> || meets_outer_fun<R>;

} // namespace cpo_set_done
} // namespace detail

inline namespace v1 {

using detail::cpo_set_done::set_done;
using detail::cpo_set_done::set_done_t;

} // namespace v1
} // namespace concore