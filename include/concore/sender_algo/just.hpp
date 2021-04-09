/**
 * @file    just.hpp
 * @brief   Definition of @ref concore::v1::just "just"
 *
 * @see     @ref concore::v1::just "just"
 */
#pragma once

#include <concore/detail/extra_type_traits.hpp>
#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <concore/_concepts/_concepts_receiver.hpp>
#include <concore/detail/sender_helpers.hpp>

#include <tuple>
#include <exception>

namespace concore {

namespace detail {

template <typename R, typename... Values>
struct just_oper {
    R receiver_;
    std::tuple<Values...> values_;

    void start() noexcept {
        try {
            auto set_value_wrapper = [&](Values&&... vals) {
                concore::set_value((R &&) receiver_, (Values &&) vals...);
            };
            std::apply(std::move(set_value_wrapper), std::move(values_));
        } catch (...) {
            concore::set_error((R &&) receiver_, std::current_exception());
        }
    }
};

template <CONCORE_CONCEPT_OR_TYPENAME(CONCORE_CONCEPT_OR_TYPENAME(detail::moveable_value))... Ts>
struct just_sender : sender_types_base<> {
    just_sender(std::tuple<Ts...> vals)
        : values_(std::move(vals)) {}

    //! The connect CPO that returns an operation state object
    template <typename R>
    just_oper<R, Ts...> connect(R&& r) && {
        static_assert(receiver<R>, "Type needs to match receiver_of concept");
        return just_oper<R, Ts...>{(R &&) r, std::move(values_)};
    }

    //! @overload
    template <typename R>
    just_oper<R, Ts...> connect(R&& r) const& {
        static_assert(receiver<R>, "Type needs to match receiver_of concept");
        return just_oper<R, Ts...>{(R &&) r, values_};
    }

private:
    std::tuple<Ts...> values_;
};

} // namespace detail

inline namespace v1 {

template <CONCORE_CONCEPT_OR_TYPENAME(detail::moveable_value)... Ts>
detail::just_sender<Ts...> just(Ts&&... values) noexcept(
        std::is_nothrow_constructible_v<std::tuple<Ts...>, Ts...>) {
    return detail::just_sender<Ts...>{(Ts &&) values...};
}

} // namespace v1
} // namespace concore
