#pragma once

#include <concore/detail/concept_macros.hpp>
#include <concore/_concepts/_concepts_sender.hpp>

namespace concore {

namespace detail {
namespace cpo_schedule {

CONCORE_DEF_REQUIRES(meets_tag_invoke,                                //
        CONCORE_LIST(typename Tag, typename S), CONCORE_LIST(Tag, S), //
        (requires(S&& s) { tag_invoke(Tag{}, (S &&) s); }),           // concepts
        tag_invoke(Tag{}, CONCORE_DECLVAL(S))                         // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_inner_fun,              //
        CONCORE_LIST(typename S), CONCORE_LIST(S), //
        (requires(S&& s) { s.schedule(); }),       // concepts
        CONCORE_DECLVAL(S).schedule()              // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_outer_fun,              //
        CONCORE_LIST(typename S), CONCORE_LIST(S), //
        (requires(S&& s) { schedule((S &&) s); }), // concepts
        schedule(CONCORE_DECLVAL(S))               // pre-concepts
);

template <typename Tag, typename S>
CONCORE_CONCEPT_OR_BOOL(has_tag_invoke) = meets_tag_invoke<Tag, S>;

inline const struct schedule_t final {
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename S), (has_tag_invoke<schedule_t, S>))
    auto operator()(S&& s) const                              //
            noexcept(noexcept(tag_invoke(*this, (S &&) s))) { //
        using res_type = decltype(tag_invoke(schedule_t{}, (S &&) s));
        static_assert(sender<res_type>, "Result type of the custom-defined schedule operation must "
                                        "model the sender concept");
        return tag_invoke(schedule_t{}, (S &&) s);
    }
} schedule{};

template <typename S>
CONCORE_CONCEPT_OR_BOOL(has_schedule) = meets_tag_invoke<schedule_t, S>;

} // namespace cpo_schedule
} // namespace detail

inline namespace v1 {

using detail::cpo_schedule::schedule;
using detail::cpo_schedule::schedule_t;

} // namespace v1

} // namespace concore
