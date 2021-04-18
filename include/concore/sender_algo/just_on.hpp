/**
 * @file    just_on.hpp
 * @brief   Definition of @ref concore::v1::just_on "just_on"
 *
 * @see     @ref concore::v1::just_on "just_on"
 */
#pragma once

#include <concore/sender_algo/just.hpp>
#include <concore/sender_algo/on.hpp>

namespace concore {

inline namespace v1 {

template <CONCORE_CONCEPT_OR_TYPENAME(scheduler) Scheduler,
        CONCORE_CONCEPT_OR_TYPENAME(detail::moveable_value)... Ts>
auto just_on(Scheduler&& sched, Ts&&... values) {
    return on(just((Ts &&) values...), (Scheduler &&) sched);
}

} // namespace v1
} // namespace concore
