/**
 * @file    as_invocable.hpp
 * @brief   Definition of @ref concore::v1::as_invocable "as_invocable"
 *
 * @see     @ref concore::v1::as_invocable "as_invocable"
 */
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
 * @details
 *
 * The receiver should model `receiver_of<>`.
 *
 * This will store a reference to the receiver; the receiver must not get out of scope.
 *
 * When this functor is called @ref set_value() will be called on the receiver. If an exception is
 * thrown, the @ref set_error() function is called.
 *
 * If the functor is never called, the destructor of this object will call set_done().
 * Alternatively, if the receiver is called manually, dismiss() can be called to prevent the
 * destructor for calling set_done().
 *
 * @see as_receiver
 */
template <typename R>
struct as_invocable {
    //! Constructor from receiver
    explicit as_invocable(R r) noexcept
        : receiver_((R &&) r) {
#if CONCORE_CXX_HAS_CONCEPTS
        static_assert(receiver_of<R>, "Type needs to match receiver_of<> concept");
#endif
    }
    //! Move constructor
    as_invocable(as_invocable&& other) noexcept
        : receiver_(std::move(other.receiver_)) {
        other.handled_ = true;
    }
    //! Move assignment
    as_invocable& operator=(as_invocable&& other) noexcept {
        receiver_ = std::move(other.receiver_);
        other.handled_ = true;
    }
    //! Copy constructor
    as_invocable(const as_invocable&) = default;
    //! Copy assignment
    as_invocable& operator=(const as_invocable&) = default;
    //! Destructor
    ~as_invocable() {
        if (!handled_)
            concore::set_done(std::move(receiver_));
    }

    /**
     * @brief Call operator
     * @details
     *
     * This forwards the call to the receiver by calling @ref set_value(). If an exception is thrown
     * during this operation, then @ref set_error() is called passing the exception.
     *
     * @see     set_value(), set_error()
     */
    void operator()() noexcept {
        handled_ = true;
        try {
            concore::set_value(std::move(receiver_));
        } catch (...) {
            concore::set_error(std::move(receiver_), std::current_exception());
        }
    }

    //! Called to dismiss the call of set_done() in the destructor.
    //! This should be called whenever the user externally calls one of the CPOs
    //! on the receiver.
    void dismiss() noexcept { handled_ = true; }

private:
    //! The receiver object to forward the call to
    R receiver_;
    bool handled_{false};
};

} // namespace v1
} // namespace concore
