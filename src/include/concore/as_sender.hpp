/**
 * @file    as_sender.hpp
 * @brief   Definition of @ref concore::v1::as_sender "as_sender"
 *
 * @see     @ref concore::v1::as_sender "as_sender"
 */
#pragma once

#include <concore/as_operation.hpp>

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
 * When this functor is called set_value() will be called on the receiver. If an exception is
 * thrown, the set_error() function is called.
 *
 * If the functor is never called, the destructor of this object will call set_done().
 *
 * This types models the @ref sender concept.
 *
 * @see sender, receiver_of, as_operation
 */
template <typename E>
struct as_sender {
    //! The value types that defines the values that this sender sends to receivers
    template <template <typename...> class Tuple, template <typename...> class Variant>
    using value_types = Variant<Tuple<>>;
    //! The type of error that this sender sends to receiver
    template <template <typename...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    //! Indicates that this sender never sends a done signal
    static constexpr bool sends_done = false;

    //! Constructor
    explicit as_sender(E e) noexcept
        : ex_((E &&) e) {
#if CONCORE_CXX_HAS_CONCEPTS
        static_assert(executor<E>, "Type needs to match executor concept");
#endif
    }

    //! The connect CPO that returns an operation state object
    template <typename R>
    as_operation<E, R> connect(R&& r) && {
#if CONCORE_CXX_HAS_CONCEPTS
        static_assert(receiver_of<R>, "Type needs to match receiver_of concept");
#endif
        return as_operation<E, R>((E &&) ex_, (R &&) r);
    }

    //! @overload
    template <typename R>
    as_operation<E, R> connect(R&& r) const& {
#if CONCORE_CXX_HAS_CONCEPTS
        static_assert(receiver_of<R>, "Type needs to match receiver_of concept");
#endif
        return as_operation<E, R>(ex_, (R &&) r);
    }

private:
    //! The wrapped executor
    E ex_;
};

} // namespace v1
} // namespace concore
