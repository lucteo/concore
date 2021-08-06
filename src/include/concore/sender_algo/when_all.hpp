/**
 * @file    when_all.hpp
 * @brief   Definition of @ref concore::v1::when_all "when_all"
 *
 * @see     @ref concore::v1::when_all "when_all"
 */
#pragma once

#include <concore/detail/extra_type_traits.hpp>
#include <concore/detail/simple_optional.hpp>
#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <concore/_cpo/_cpo_set_done.hpp>
#include <concore/_cpo/_cpo_start.hpp>
#include <concore/_cpo/_cpo_connect.hpp>
#include <concore/_concepts/_concepts_sender.hpp>
#include <concore/detail/sender_helpers.hpp>

#include <exception>
#include <tuple>
#include <atomic>
#include <cassert>

namespace concore {

namespace detail {

template <typename F, typename Tuple, size_t... I>
auto apply_on_tuple_impl(F&& f, Tuple&& t, std::index_sequence<I...>) {
    return ((F &&) f)(std::get<I>((Tuple &&) t)...);
}

//! Reimplementation of std::apply, to ensure compatibility with older compilers
template <typename F, typename Tuple>
auto apply_on_tuple(F&& f, Tuple&& t) {
    constexpr size_t N = std::tuple_size<remove_cvref_t<Tuple>>::value;
    return apply_on_tuple_impl((F &&) f, (Tuple &&) t, std::make_index_sequence<N>{});
}

//! Structure that holds the operations and the receiver data created for each incoming sender.
//! It recursively stores all the needed operations and resulting values.
template <typename Parent, typename... Ss>
struct opers_tuple;

template <typename Parent, typename CurSender, typename... Rest>
struct opers_tuple<Parent, CurSender, Rest...> : opers_tuple<Parent, Rest...> {
private:
    //! The base class containing the rest of the senders
    using base_t = opers_tuple<Parent, Rest...>;

    using cur_val_t = sender_single_return_type<remove_cvref_t<CurSender>>;
    using cur_val_opt_t = simple_optional<cur_val_t>;

    //! Receiver to be called by CurSender
    struct one_receiver {
        //! The parent operation state, the one that contains all the state for when_all
        Parent* parent_;
        //! Pointer to where the value send in this receiver will be stored
        cur_val_opt_t* cur_val_;

        one_receiver(Parent* parent, cur_val_opt_t* cur_val)
            : parent_(parent)
            , cur_val_(cur_val) {}

        template <typename T>
        void set_value(T&& val) noexcept {
            //! Store the given value
            cur_val_->emplace((T &&) val);
            //! Tell the parent that we set tje value
            parent_->on_set_value();
        }
        void set_done() noexcept { parent_->on_set_done(); }
        void set_error(std::exception_ptr eptr) noexcept { parent_->on_set_error(eptr); }
    };

    //! The value that would be sent by the current sender to our receiver
    cur_val_opt_t cur_val_;

    //! The operation state for the current sender connected to our receiver
    connect_result_t<CurSender, one_receiver> op_;

public:
    //! Constructs all the operations, connecting all the senders with receivers
    opers_tuple(Parent* parent, CurSender&& fs, Rest&&... rest)
        : base_t(parent, (Rest &&) rest...)
        , cur_val_()
        , op_(concore::connect((CurSender &&) fs, one_receiver{parent, &cur_val_})) {}

    //! Start all the operations for all the incoming senders
    void start() noexcept {
        concore::start(std::move(op_));
        base_t::start();
    }

    //! Called to collect all stored values and push them to the final receiver.
    //! The final receiver will be given as the first argument to this call
    template <typename... Prev>
    void push_values(Prev&&... prev) {
        assert(cur_val_.isSet_);
        base_t::push_values((Prev &&) prev..., cur_val_.value());
    }
};

template <typename Parent>
struct opers_tuple<Parent> {
    opers_tuple(Parent*) {}

    void start() noexcept {}

    template <typename... Args>
    void push_values(Args&&... args) {
        concore::set_value((Args &&) args...);
    }
};

template <typename Receiver, typename... Ss>
struct when_all_sender_oper_state {
    when_all_sender_oper_state(Receiver r, Ss... ss)
        : incoming_opers_(this, (Ss &&) ss...)
        , finalReceiver_((Receiver &&) r) {}

    void start() noexcept {
        ref_count_ = sizeof...(Ss);
        incoming_opers_.start();
    }

    void on_set_value() noexcept { on_incoming_complete(); }

    void on_set_done() noexcept {
        int old = 0;
        state_.compare_exchange_strong(old, 2);
        on_incoming_complete();
    }

    void on_set_error(std::exception_ptr eptr) noexcept {
        int old = 0;
        if (state_.compare_exchange_strong(old, 1))
            eptr_ = std::move(eptr);
        on_incoming_complete();
    }

private:
    //! The operations for all the incoming senders
    opers_tuple<when_all_sender_oper_state, Ss...> incoming_opers_;
    //! The number of receivers we are still waiting on
    std::atomic<int> ref_count_{0};
    //! The first exception that was generated, if any
    std::exception_ptr eptr_{};
    //! The state of the current data
    std::atomic<int> state_{0}; // 0 == values/waiting, 1 == error, 2 == done
    //! The receiver of
    Receiver finalReceiver_;

    //! Called after an incoming sender sends a value/done/error to us, and we handle it
    //! Decrements the ref count. If this is the last incoming sender to finish, then push the
    //! values/done/error to the final receiver
    void on_incoming_complete() {
        if (ref_count_-- == 1) {
            int state = state_.load();
            if (state == 0) {
                // The values are stored near the incoming operations. Call our recursive structure
                // to push the values to the receiver. Pass the receiver as the first argument.
                // This will result in: `concore::set_value(finalReceiver_, val1, val2, val3, ...)`
                incoming_opers_.push_values((Receiver &&) finalReceiver_);
            } else if (state == 1)
                concore::set_error((Receiver &&) finalReceiver_, std::move(eptr_));
            else
                concore::set_done((Receiver &&) finalReceiver_);
        }
    }
};

template <CONCORE_CONCEPT_OR_TYPENAME(typed_sender)... Ss>
struct when_all_sender {
    //! The value types that defines the values that this sender sends to receivers
    template <template <typename...> class Tuple, template <typename...> class Variant>
    using value_types = Variant<Tuple<sender_single_return_type<remove_cvref_t<Ss>>...>>;

    //! The type of error that this sender sends to receiver
    template <template <typename...> class Variant>
    using error_types = Variant<std::exception_ptr>;

    //! Indicates whether this sender can send "done" signal
    static constexpr bool sends_done = true;

    explicit when_all_sender(Ss&&... sender)
        : incomingSenders_((Ss &&) sender...) {}

    //! The connect CPO that returns an operation state object
    template <typename R>
    when_all_sender_oper_state<R, Ss...> connect(R&& r) && {
        static_assert(receiver<R>, "Given object is not a receiver");
        auto f = [&](Ss && ... ss) -> when_all_sender_oper_state<R, Ss...> {
            return {(R &&) r, (Ss &&) ss...};
            ;
        };
        return apply_on_tuple(std::move(f), std::move(incomingSenders_));
    }

    //! @overload
    template <typename R>
    when_all_sender_oper_state<R, Ss...> connect(R&& r) const& {
        static_assert(receiver<R>, "Given object is not a receiver");
        auto f = [&](const Ss&... ss) -> when_all_sender_oper_state<R, Ss...> {
            return {(R &&) r, (Ss &&) ss...};
        };
        return apply_on_tuple(std::move(f), incomingSenders_);
    }

private:
    //! The incoming senders
    std::tuple<Ss...> incomingSenders_;
};

} // namespace detail

inline namespace v1 {

template <CONCORE_CONCEPT_OR_TYPENAME(typed_sender)... Ss>
auto when_all(Ss&&... ss) {
    return detail::when_all_sender<Ss...>{(Ss &&) ss...};
}

} // namespace v1
} // namespace concore
