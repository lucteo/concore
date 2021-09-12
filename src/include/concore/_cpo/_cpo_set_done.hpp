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

template <typename Tag, typename R>
CONCORE_CONCEPT_OR_BOOL(has_tag_invoke) = meets_tag_invoke<Tag, R>;

inline const struct set_done_t final {
    CONCORE_TEMPLATE_COND(typename R, (has_tag_invoke<set_done_t, R>))
    void operator()(R&& r) const                              //
            noexcept(noexcept(tag_invoke(*this, (R &&) r))) { //
        tag_invoke(set_done_t{}, (R &&) r);
    }
} set_done{};

template <typename R>
CONCORE_CONCEPT_OR_BOOL(has_set_done) = meets_tag_invoke<set_done_t, R>;

} // namespace cpo_set_done
} // namespace detail

inline namespace v1 {

using detail::cpo_set_done::set_done;
using detail::cpo_set_done::set_done_t;

} // namespace v1
} // namespace concore