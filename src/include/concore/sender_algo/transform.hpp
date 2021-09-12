/**
 * @file    transform.hpp
 * @brief   Definition of @ref concore::v1::transform "transform"
 *
 * @see     @ref concore::v1::transform "transform"
 */
#pragma once

#include <concore/detail/extra_type_traits.hpp>
#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <concore/_cpo/_cpo_start.hpp>
#include <concore/_concepts/_concepts_sender.hpp>
#include <concore/detail/sender_helpers.hpp>

#include <exception>

namespace concore {

namespace detail {

template <typename PrevSender, typename Receiver, typename F>
struct transform_sender_oper_state {
    //! Receiver called by the scheduler sender to start the actual operation.
    //! In this we will move the receiver and the sender objects given to `on`.
    struct prev_receiver {
        //! The final receiver to get notified by the sender
        Receiver receiver_;
        //! The function called to transform the results of the previous sender
        F f_;

        prev_receiver(Receiver receiver, F f)
            : receiver_((Receiver &&) receiver)
            , f_((F &&) f) {}

        template <typename... Ts>
        friend void tag_invoke(set_value_t, prev_receiver&& self, Ts... vals) noexcept {
            try {
                using result_type = std::invoke_result_t<F, Ts...>;
                if constexpr (std::is_void_v<result_type>) {
                    self.f_((Ts &&) vals...);
                    concore::set_value((Receiver &&) self.receiver_);
                } else
                    concore::set_value((Receiver &&) self.receiver_, self.f_((Ts &&) vals...));
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

    transform_sender_oper_state(PrevSender sender, Receiver receiver, F f)
        : entryOpState_(concore::connect(
                  (PrevSender &&) sender, prev_receiver{(Receiver &&) receiver, (F &&) f})) {}

    void start() noexcept { concore::start(std::move(entryOpState_)); }
};

template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender, typename F, typename Res>
struct transform_sender : sender_types_base<Sender::sends_done, Res> {
    transform_sender(Sender sender, F f)
        : sender_((Sender &&) sender)
        , f_((F &&) f) {}

    //! The connect CPO that returns an operation state object
    template <typename R>
    transform_sender_oper_state<Sender, R, F> connect(R&& r) && {
        static_assert(receiver<R>, "Given object is not a receiver");
        return {(Sender &&) sender_, (R &&) r, (F &&) f_};
    }

    //! @overload
    template <typename R>
    transform_sender_oper_state<Sender, R, F> connect(R&& r) const& {
        static_assert(receiver<R>, "Given object is not a receiver");
        return {sender_, (R &&) r, f_};
    }

private:
    Sender sender_;
    F f_;
};

template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender, typename F>
auto create_transform_sender(Sender&& s, F&& f) {
    using res_t = transform_return_type<remove_cvref_t<Sender>, F>;
    return transform_sender<remove_cvref_t<Sender>, F, res_t>{(Sender &&) s, (F &&) f};
}

template <typename F>
struct transform_create_fun {
    F f_;

    explicit transform_create_fun(F&& f)
        : f_((F &&) f) {}

    template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender>
    auto operator()(Sender&& sender) const& {
        return detail::create_transform_sender((Sender &&) sender, f_);
    }

    template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender>
    auto operator()(Sender&& sender) && {
        return detail::create_transform_sender((Sender &&) sender, (F &&) f_);
    }
};

} // namespace detail

inline namespace v1 {

template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender, typename F>
auto transform(Sender&& s, F&& f) {
    return detail::create_transform_sender((Sender &&) s, (F &&) f);
}

template <typename F>
auto transform(F&& f) {
    return detail::make_sender_algo_wrapper(detail::transform_create_fun<F>{(F &&) f});
}

} // namespace v1
} // namespace concore
