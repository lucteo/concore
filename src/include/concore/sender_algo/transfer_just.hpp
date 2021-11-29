/**
 * @file    transfer_just.hpp
 * @brief   Definition of @ref concore::v1::transfer_just "transfer_just"
 *
 * @see     @ref concore::v1::transfer_just "transfer_just"
 */
#pragma once

#include <concore/sender_algo/just.hpp>
#include <concore/sender_algo/on.hpp>

namespace concore {

inline namespace v1 {

template <CONCORE_CONCEPT_OR_TYPENAME(scheduler) Scheduler,
        CONCORE_CONCEPT_OR_TYPENAME(detail::moveable_value)... Ts>
auto transfer_just(Scheduler&& sched, Ts&&... values) {
    return on(just((Ts &&) values...), (Scheduler &&) sched);
}

} // namespace v1
} // namespace concore
