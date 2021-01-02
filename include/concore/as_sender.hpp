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
 * The receiver should model receiver_of<>.
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
template <CONCORE_CONCEPT_OR_TYPENAME(executor) E>
struct as_sender {
    template <template <typename...> class Tuple, template <typename...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <typename...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = false;

    explicit as_sender(E e) noexcept
        : ex_((E &&) e) {}

    template <CONCORE_CONCEPT_OR_TYPENAME(receiver_of) R>
    as_operation<E, R> connect(R&& r) && {
        return as_operation<E, R>((E &&) ex_, (R &&) r);
    }

    template <CONCORE_CONCEPT_OR_TYPENAME(receiver_of) R>
    as_operation<E, R> connect(R&& r) const& {
        return as_operation<E, R>(ex_, (R &&) r);
    }

private:
    E ex_;
};

} // namespace v1
} // namespace concore
