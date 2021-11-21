#pragma once

#include <concore/detail/concept_macros.hpp>

namespace concore {

namespace detail {
namespace cpo_start {

CONCORE_DEF_REQUIRES(meets_tag_invoke,                                //
        CONCORE_LIST(typename Tag, typename O), CONCORE_LIST(Tag, O), //
        (requires(O& o) { tag_invoke(Tag{}, (O&)o); }),               // concepts
        tag_invoke(Tag{}, CONCORE_DECLVALREF(O))                      // pre-concepts
);

template <typename Tag, typename O>
CONCORE_CONCEPT_OR_BOOL(has_tag_invoke) = meets_tag_invoke<Tag, concore::detail::remove_cvref_t<O>>;

inline const struct start_t final {
    CONCORE_TEMPLATE_COND(typename O, (has_tag_invoke<start_t, O>))
    void operator()(O& o) const                        //
            noexcept(noexcept(tag_invoke(*this, o))) { //
        tag_invoke(start_t{}, o);
    }
} start{};

template <typename O>
CONCORE_CONCEPT_OR_BOOL(has_start) = meets_tag_invoke<start_t, concore::detail::remove_cvref_t<O>>;

} // namespace cpo_start
} // namespace detail

inline namespace v1 {

using detail::cpo_start::start;
using detail::cpo_start::start_t;

} // namespace v1
} // namespace concore