#pragma once

#include <concore/_concepts/_concepts_receiver.hpp>

#include <utility>

namespace concore {
inline namespace v1 {

/**
 * @brief   Wrapper that transforms a receiver into a functor
 *
 * @tparam  R The type of the receiver
 *
 * The receiver should model receivereceiver_of<>.
 *
 * This will store a reference to the receiver; the receiver must not get out of scope.
 *
 * When this functor is called set_value() will be called on the receiver. If an exception is
 * thrown, the set_error() function is called.
 *
 * If the functor is never called, the destructor of this object will call set_done().
 *
 * @see as_receiver
 */
CONCORE_TEMPLATE_COND(typename R, receiver_of<R>)
struct as_invocable {
    explicit as_invocable(R& r) noexcept
        : receiver_(&r) {}
    explicit as_invocable(as_invocable&& other) noexcept
        : receiver_(std::move(other.receiver_)) {
        other.receiver_ = nullptr;
    }
    as_invocable& operator=(as_invocable&& other) noexcept {
        receiver_ = other.receiver_;
        other.receiver_ = nullptr;
    }
    as_invocable(const as_invocable&) = delete;
    as_invocable& operator=(const as_invocable&) = delete;
    ~as_invocable() {
        if (receiver_)
            concore::set_done(std::move(*receiver_));
    }
    void operator()() noexcept {
        try {
            concore::set_value(std::move(*receiver_));
            receiver_ = nullptr;
        } catch (...) {
            concore::set_error(std::move(*receiver_), std::current_exception());
            receiver_ = nullptr;
        }
    }

private:
    R* receiver_;
};

} // namespace v1
} // namespace concore
