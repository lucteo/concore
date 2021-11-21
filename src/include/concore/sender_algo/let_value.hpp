/**
 * @file    let_value.hpp
 * @brief   Definition of @ref concore::v1::let_value "let_value"
 *
 * @see     @ref concore::v1::let_value "let_value"
 */
#pragma once

#include <concore/detail/extra_type_traits.hpp>
#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <concore/_cpo/_cpo_set_done.hpp>
#include <concore/_cpo/_cpo_connect.hpp>
#include <concore/_cpo/_cpo_start.hpp>
#include <concore/_concepts/_concepts_sender.hpp>
#include <concore/detail/sender_helpers.hpp>
#include <concore/detail/simple_optional.hpp>

#include <exception>

namespace concore {

namespace detail {

template <typename BaseOp, typename Storage>
struct oper_state_with_storage {
    Storage storage_;
    simple_optional<BaseOp> base_oper_{};

    explicit oper_state_with_storage(Storage s)
        : storage_((Storage &&) s) {}

    template <typename... Ts>
    explicit oper_state_with_storage(Ts... vals)
        : storage_((Ts &&) vals...) {}

    friend void tag_invoke(start_t, oper_state_with_storage& self) noexcept {
        concore::start(self.base_oper_.value());
    }
};

template <typename PrevSender, typename Receiver, typename F, typename ResSender, typename Storage>
struct let_value_sender_oper_state {
    //! Receiver called by the scheduler sender to start the actual operation.
    //! In this we will move the receiver and the sender objects given to `on`.
    struct prev_receiver {
        //! The final receiver to get notified by the sender
        Receiver receiver_;
        //! The function called to let_value the results of the previous sender
        F f_;

        prev_receiver(Receiver receiver, F f)
            : receiver_((Receiver &&) receiver)
            , f_((F &&) f) {}

        template <typename... Ts>
        friend void tag_invoke(set_value_t, prev_receiver&& self, Ts... vals) noexcept {
            try {
                // Create an oper_state wrapper that will pack the base oper_state and our needed
                // storage
                using base_op_t = decltype( //
                        concore::connect(CONCORE_DECLVAL(ResSender), (Receiver &&) self.receiver_));
                oper_state_with_storage<base_op_t, Storage> oper_wrapper{(Ts &&) vals...};

                // Call the given functor and generate the base operation we need to execute
                auto res_sender = ((F &&) self.f_)(oper_wrapper.storage_);
                oper_wrapper.base_oper_.emplace(
                        concore::connect(std::move(res_sender), (Receiver &&) self.receiver_));

                // Start the wrapped operation
                // The storage will be kept alive for the whole duration of the operation
                concore::start(oper_wrapper);
            } catch (...) {
                concore::set_error((Receiver &&) self.receiver_, std::current_exception());
            }
        }
        friend void tag_invoke(set_done_t, prev_receiver&& self) noexcept {
            concore::set_done((Receiver &&) self.receiver_);
        }
        friend void tag_invoke(
                set_error_t, prev_receiver&& self, std::exception_ptr eptr) noexcept {
            concore::set_error((Receiver &&) self.receiver_, eptr);
        }
    };

    //! The operation state used for connecting the previous sender with our receiver
    connect_result_t<PrevSender, prev_receiver> entryOpState_;

    let_value_sender_oper_state(PrevSender sender, Receiver receiver, F f)
        : entryOpState_(concore::connect(
                  (PrevSender &&) sender, prev_receiver{(Receiver &&) receiver, (F &&) f})) {}

    friend void tag_invoke(start_t, let_value_sender_oper_state& self) noexcept {
        concore::start(self.entryOpState_);
    }
};

template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender, typename F, typename ResSender,
        typename Storage>
struct let_value_sender {
    //! The value types that defines the values that this sender sends to receivers
    template <template <typename...> class Tuple, template <typename...> class Variant>
    using value_types = typename ResSender::template value_types<Tuple, Variant>;

    //! The type of error that this sender sends to receiver
    template <template <typename...> class Variant>
    using error_types = Variant<std::exception_ptr>;

    //! Indicates whether this sender can send "done" signal
    static constexpr bool sends_done = Sender::sends_done || ResSender::sends_done;

    let_value_sender(Sender sender, F f)
        : sender_((Sender &&) sender)
        , f_((F &&) f) {}

    //! The connect CPO that returns an operation state object
    template <typename R>
    friend let_value_sender_oper_state<Sender, R, F, ResSender, Storage> tag_invoke(
            connect_t, let_value_sender&& s, R&& r) {
        static_assert(receiver<R>, "Given object is not a receiver");
        return {(Sender &&) s.sender_, (R &&) r, (F &&) s.f_};
    }
    //! @overload
    template <typename R>
    friend let_value_sender_oper_state<Sender, R, F, ResSender, Storage> tag_invoke(
            connect_t, const let_value_sender& s, R&& r) {
        static_assert(receiver<R>, "Given object is not a receiver");
        return {s.sender_, (R &&) r, s.f_};
    }

private:
    Sender sender_;
    F f_;
};

template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender, typename F>
auto create_let_value_sender(Sender&& s, F&& f) {
    using res_t = detail::transform_ref_return_type<Sender, F>;
    using val_t = detail::sender_single_return_type<Sender>;
    return detail::let_value_sender<Sender, F, res_t, val_t>{(Sender &&) s, (F &&) f};
}

template <typename F>
struct let_value_create_fun {
    F f_;

    explicit let_value_create_fun(F&& f)
        : f_((F &&) f) {}

    template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender>
    auto operator()(Sender&& sender) const& {
        return detail::create_let_value_sender((Sender &&) sender, f_);
    }

    template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender>
    auto operator()(Sender&& sender) && {
        return detail::create_let_value_sender((Sender &&) sender, (F &&) f_);
    }
};

} // namespace detail

inline namespace v1 {

template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender, typename F>
auto let_value(Sender&& s, F&& f) {
    return detail::create_let_value_sender((Sender &&) s, (F &&) f);
}

template <typename F>
auto let_value(F&& f) {
    return detail::make_sender_algo_wrapper(detail::let_value_create_fun<F>{(F &&) f});
}

} // namespace v1
} // namespace concore
