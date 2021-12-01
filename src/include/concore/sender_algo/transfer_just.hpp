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

/**
 * @brief   Returns a sender that propagates a set of values on the execution context represented by
 *          the given scheduler.
 *
 * @tparam Scheduler    The type of scheduler we are using
 * @tparam Ts           The type of values to be sent in the context of the scheduler
 *
 * @param sched     The scheduler object representing the execution context in which we want to send
 *                  the values.
 * @param values    The values we want to send in the given execution context
 *
 * @return  A sender that sends the values `values` in the context of `sched`
 *
 * @details
 *
 * This will return a sender that will transfer the given values on the execution context
 * represented by the given scheduler, and passes them to the connected receiver in that context.
 *
 * This is equivalent to `transfer(just(values...), sched)`.
 *
 * This will obtain a sender from the given scheduler and connects it to an internal receiver.
 * Whenever that receiver gets the @ref set_value() notification, this will call @ref set_value()
 * with the values received as parameters, in the context of `sched`.
 *
 * Let `snd` be the sender that can be obtained from `sched`, and `S` its type.
 *
 * Preconditions:
 *  - `Scheduler` type models `scheduler` concept
 *  - all types in `Ts` satisfy `std::moveable-value`
 *
 * Postconditions:
 *  - returned type is models `sender` concept
 *  - returned sender will yield values `value` when connected to an appropriate receiver and
 *    started
 *  - returned sender will call @ref set_error() if calling @ref set_values with the given values
 *    throws
 *  - returned sender will call @ref set_error() if `snd` calls @ref set_error(), forwarding the
 * error type
 *  - returned sender calls @ref set_done() if `snd` calls @ref set_done()
 *  - the returned sender has `value_types` = `Variant<Tuple<Ts...>>`
 *  - the returned sender has `error_types` = `S::error_types`
 *  - the returned sender has `sends_done` = `S::sends_done`
 *  - the returned sender advertises the completion scheduler, which is `sched`
 *
 * @see just(), transfer()
 */
template <CONCORE_CONCEPT_OR_TYPENAME(scheduler) Scheduler,
        CONCORE_CONCEPT_OR_TYPENAME(detail::moveable_value)... Ts>
auto transfer_just(Scheduler&& sched, Ts&&... values) {
    return on(just((Ts &&) values...), (Scheduler &&) sched);
}

} // namespace v1
} // namespace concore
