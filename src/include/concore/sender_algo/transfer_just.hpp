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

namespace detail {

template <typename Tag, typename Sched, typename... Vs>
CONCORE_CONCEPT_OR_BOOL has_transfer_just = tag_invocable<Tag, Sched, Vs...>;

inline const struct transfer_just_t final {
    // User defined tag_invoke for transfer-just
    CONCORE_TEMPLATE_COND(CONCORE_LIST(                                                       //
                                  CONCORE_CONCEPT_OR_TYPENAME(scheduler) Sched,               //
                                  CONCORE_CONCEPT_OR_TYPENAME(detail::moveable_value)... Vs), //
            (has_transfer_just<transfer_just_t, Sched, Vs...>))
    auto operator()(Sched&& sched, Vs... vs) const                           //
            noexcept(nothrow_tag_invocable<transfer_just_t, Sched, Vs...>) { //
        using res_type = decltype(tag_invoke(transfer_just_t{}, (Sched &&) sched, (Vs &&) vs...));
        static_assert(typed_sender<res_type>,
                "Result type of the custom-defined transfer_just operation must "
                "model the typed_sender concept");
        return tag_invoke(transfer_just_t{}, (Sched &&) sched, (Vs &&) vs...);
    }

    // Default transfer_just implementation
    CONCORE_TEMPLATE_COND(CONCORE_LIST(                                                       //
                                  CONCORE_CONCEPT_OR_TYPENAME(scheduler) Sched,               //
                                  CONCORE_CONCEPT_OR_TYPENAME(detail::moveable_value)... Vs), //
            (!has_transfer_just<transfer_just_t, Sched, Vs...>))
    auto operator()(Sched&& sched, Vs... vs) const                           //
            noexcept(nothrow_tag_invocable<transfer_just_t, Sched, Vs...>) { //
        return on(just((Vs &&) vs...), (Sched &&) sched);
    }
} transfer_just{};

} // namespace detail

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
 * The user can provide its own custom implementation for transfer_just, by providing a tag_invoke
 * specialization for this object.
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
#if DOXYGEN_BUILD
template <scheduler Scheduler, detail::moveable_value... Ts>
auto transfer_just(Scheduler&& sched, Ts&&... values);
#endif

using detail::transfer_just;

} // namespace v1
} // namespace concore
