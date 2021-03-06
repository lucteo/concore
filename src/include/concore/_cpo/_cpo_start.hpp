#pragma once

#include <concore/detail/concept_macros.hpp>

namespace concore {

namespace detail {
namespace cpo_start {

CONCORE_DEF_REQUIRES(meets_tag_invoke,                                //
        CONCORE_LIST(typename Tag, typename O), CONCORE_LIST(Tag, O), //
        (requires(O&& o) { tag_invoke(Tag{}, (O &&) o); }),           // concepts
        tag_invoke(Tag{}, CONCORE_DECLVAL(O))                         // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_inner_fun,     //
        typename O, O,                    //
        (requires(O&& o) { o.start(); }), // concepts
        CONCORE_DECLVAL(O).start()        // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_outer_fun,           //
        typename O, O,                          //
        (requires(O&& o) { start((O &&) o); }), // concepts
        start(CONCORE_DECLVAL(O))               // pre-concepts
);

template <typename Tag, typename O>
CONCORE_CONCEPT_OR_BOOL(has_tag_invoke) = meets_tag_invoke<Tag, O>;

template <typename Tag, typename O>
CONCORE_CONCEPT_OR_BOOL(has_inner_fun) = !has_tag_invoke<Tag, O> && meets_inner_fun<O>;

template <typename Tag, typename O>
CONCORE_CONCEPT_OR_BOOL(has_outer_fun) = !has_tag_invoke<Tag, O> &&
                                         !has_inner_fun<Tag, O> && meets_outer_fun<O>;

inline const struct start_t final {
    CONCORE_TEMPLATE_COND(typename O, (has_tag_invoke<start_t, O>))
    void operator()(O&& o) const                              //
            noexcept(noexcept(tag_invoke(*this, (O &&) o))) { //
        tag_invoke(start_t{}, (O &&) o);
    }
    CONCORE_TEMPLATE_COND(typename O, (has_inner_fun<start_t, O>))
    void operator()(O&& o) const                     //
            noexcept(noexcept(((O &&) o).start())) { //
        ((O &&) o).start();
    }
    CONCORE_TEMPLATE_COND(typename O, (has_outer_fun<start_t, O>))
    void operator()(O&& o) const                  //
            noexcept(noexcept(start((O &&) o))) { //
        start((O &&) o);
    }
} start{};

template <typename O>
CONCORE_CONCEPT_OR_BOOL(
        has_start) = meets_tag_invoke<start_t, O> || meets_inner_fun<O> || meets_outer_fun<O>;

} // namespace cpo_start
} // namespace detail

inline namespace v1 {

using detail::cpo_start::start;
using detail::cpo_start::start_t;

} // namespace v1
} // namespace concore