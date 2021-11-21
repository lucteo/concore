#pragma once

#include <concore/_cpo/_tag_invoke.hpp>
#include <concore/_concepts/_concepts_sender.hpp>

namespace concore {

namespace detail {
namespace cpo_schedule {

inline const struct schedule_t final {
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename S), //
            (tag_invocable<schedule_t, S>))
    auto operator()(S&& s) const                             //
            noexcept(nothrow_tag_invocable<schedule_t, S>) { //
        using res_type = decltype(tag_invoke(schedule_t{}, (S &&) s));
        static_assert(sender<res_type>, "Result type of the custom-defined schedule operation must "
                                        "model the sender concept");
        return tag_invoke(schedule_t{}, (S &&) s);
    }
} schedule{};

template <typename S>
CONCORE_CONCEPT_OR_BOOL has_schedule = tag_invocable<schedule_t, S>;

} // namespace cpo_schedule
} // namespace detail

inline namespace v1 {

using detail::cpo_schedule::schedule;
using detail::cpo_schedule::schedule_t;

} // namespace v1

} // namespace concore
