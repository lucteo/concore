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
 * @brief   Wrapper that transforms an executor into a sender
 *
 * @tparam  E The type of the executor
 *
 * @details
 *
 * The executor should model `executor<>`.
 *
 * When provided with a receiver (that takes no value), this will create an operation state that
 * will call the receiver. It will call `set_value()`. If the enqueueing of the task into the
 * executor throws, it will call `set_error()`. If the executor cannot execute any tasks, e.g., it
 * was stopped, then `set_done()` will be called.
 *
 * This will store a reference to the receiver; the receiver must not get out of scope.
 *
 * This types models the @ref sender concept.
 *
 * @see sender, executor, as_operation
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
