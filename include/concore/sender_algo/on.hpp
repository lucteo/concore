/**
 * @file    on.hpp
 * @brief   Definition of @ref concore::v1::on "on"
 *
 * @see     @ref concore::v1::on "on"
 */
#pragma once

#include <concore/detail/extra_type_traits.hpp>
#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <concore/_cpo/_cpo_connect.hpp>
#include <concore/_cpo/_cpo_submit.hpp>
#include <concore/_cpo/_cpo_start.hpp>
#include <concore/_cpo/_cpo_schedule.hpp>
#include <concore/_concepts/_concept_scheduler.hpp>
#include <concore/_concepts/_concepts_sender.hpp>
#include <concore/_concepts/_concept_sender_to.hpp>
#include <concore/detail/sender_helpers.hpp>

#include <exception>
#include <optional>
#include <type_traits>
#include <memory>

namespace concore {

namespace detail {

template <typename Sender, typename SchedulerSender, typename Receiver>
struct on_sender_oper_state {
    //! Receiver called by the scheduler sender to start the actual operation.
    //! In this we will move the receiver and the sender objects given to `on`.
    struct sched_receiver {
        //! The final receiver to get notified by the sender
        Receiver receiver_;
        //! The sender that sends the signal to the receiver, hopefully on the scheduler thread
        Sender sender_;

        sched_receiver(Receiver receiver, Sender sender)
            : receiver_((Receiver &&) receiver)
            , sender_((Sender &&) sender) {}

        void set_value() noexcept {
            try {
                concore::submit((Sender &&) sender_, (Receiver &&) receiver_);
            } catch (...) {
                concore::set_error((Receiver &&) receiver_, std::current_exception());
            }
        }
        void set_done() noexcept { concore::set_done((Receiver &&) receiver_); }
        void set_error(std::exception_ptr eptr) noexcept {
            concore::set_error((Receiver &&) receiver_, eptr);
        }
    };

    //! The operation state used for the scheduler call
    connect_result_t<SchedulerSender, sched_receiver> schedOpState_;

    //! Constructor. Builds the operation to be started on the scheduler
    on_sender_oper_state(Receiver receiver, Sender sender, SchedulerSender schedSender)
        : schedOpState_(concore::connect((SchedulerSender &&) schedSender,
                  sched_receiver{(Receiver &&) receiver, (Sender &&) sender})) {}

    //! Called by the scheduler (hopefully on the scheduler thread) to send value from the given
    //! sender to the given receiver
    void start() noexcept { concore::start(std::move(schedOpState_)); }
};

template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender,
        CONCORE_CONCEPT_OR_TYPENAME(sender) SchedSender>
struct on_sender {
    //! The value types that defines the values that this sender sends to receivers
    template <template <typename...> class Tuple, template <typename...> class Variant>
    using value_types = typename Sender::template value_types<Tuple, Variant>;

    //! The type of error that this sender sends to receiver
    template <template <typename...> class Variant>
    using error_types = Variant<std::exception_ptr>;

    //! Indicates whether this sender can send "done" signal
    static constexpr bool sends_done = SchedSender::sends_done || Sender::sends_done;

    on_sender(Sender sender, SchedSender schedSender)
        : sender_((Sender &&) sender)
        , schedSender_((SchedSender &&) schedSender) {}

    //! The connect CPO that returns an operation state object
    template <typename R>
    on_sender_oper_state<Sender, SchedSender, R> connect(R&& r) && {
        static_assert(sender_to<Sender, R>, "Given receiver doesn't match the sender");
        return {(R &&) r, (Sender &&) sender_, (SchedSender &&) schedSender_};
    }

    //! @overload
    template <typename R>
    on_sender_oper_state<Sender, SchedSender, R> connect(R&& r) const& {
        static_assert(sender_to<Sender, R>, "Given receiver doesn't match the sender");
        return {(R &&) r, sender_, schedSender_};
    }

private:
    Sender sender_;
    SchedSender schedSender_;
};

} // namespace detail

inline namespace v1 {

template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender,
        CONCORE_CONCEPT_OR_TYPENAME(scheduler) Scheduler>
auto on(Sender s, Scheduler sch) {
    auto schedSender = concore::schedule((Scheduler &&) sch);
    using SchedSender = decltype(schedSender);
    return detail::on_sender<Sender, SchedSender>{(Sender &&) s, (SchedSender &&) schedSender};
}

} // namespace v1
} // namespace concore
