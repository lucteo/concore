#pragma once

#include <concore/rxs/_cpo_start_with.hpp>
#include <concore/detail/cxx_features.hpp>
#include <concore/detail/extra_type_traits.hpp>
#include <concore/detail/concept_macros.hpp>

#include <type_traits>

namespace concore {

namespace rxs {

namespace detail {

template <typename R>
struct receiver_archetype {
    void set_value(R) {}
    void set_done() noexcept {}
    void set_error(std::exception_ptr) noexcept {}

    friend inline bool operator==(receiver_archetype, receiver_archetype) { return false; }
    friend inline bool operator!=(receiver_archetype, receiver_archetype) { return true; }
};

template <>
struct receiver_archetype<void> {
    void set_value() {}
    void set_done() noexcept {}
    void set_error(std::exception_ptr) noexcept {}

    friend inline bool operator==(receiver_archetype, receiver_archetype) { return false; }
    friend inline bool operator!=(receiver_archetype, receiver_archetype) { return true; }
};

struct error_type {};

error_type stream_value_type_tester(...);
template <typename S, typename R = typename S::value_type>
S stream_value_type_tester(S*);

template <typename S>
using stream_value_type = decltype(stream_value_type_tester(static_cast<S*>(nullptr)));

template <typename S, typename R = stream_value_type<S>>
CONCORE_CONCEPT_OR_BOOL(stream_impl) =                                       //
        (std::is_copy_constructible_v<S>)                                    //
        &&(!std::is_same_v<R, error_type>)                                   //
        &&(detail::cpo_start_with::has_start_with<S, receiver_archetype<R>>) //
        ;

} // namespace detail

inline namespace v1 {

/**
 * @brief Concept modeling types that represent streams of values.
 *
 * @tparam S The type to check if it matches the concept
 *
 * @details
 *
 * A type S models stream if objects of type S can represent streams that send values of a given
 * type, or void towards receivers. The type values sent by the stream object will be indicated by
 * the inner type `value_type`; this can be void. Semantically, one can perform two operations with
 * such a computation object:
 * - start the stream (i.e., telling it to start producing values)
 * - make use of the yielded values by being able to connect it to object that receive value
 * notifications.
 *
 * If computations can ony send one value, a stream sends multiple values to the sender. While
 * computing the next value there might be errors, or some computations might be cancelled. In this
 * case the stream calls set_error() and set_done() CPOs on the receiver. To signal the presence of
 * errors (calling set_error()) might happen multiple times in the lifetime of the stream. On the
 * other hand, the cancellation of the stream can happen only once. If a stream sends a cancellation
 * notification (i.e., calls set_done()) it will not invoke the receiver anymore. It is expected
 * that the streams will always send a send_done() after the last value yielded from the stream.
 * This way, the receiver might release resources that it has acquired.
 *
 * When started with a receiver, the stream will take ownership (one way or another) of the given
 * receiver. The receiver is destroyed when the stream commands it.
 *
 * The streams objects when created are inactive, and do not yield any values. They need to be
 * "started" in order to become active. The receiver object that will receive all the notifications
 * is passed in whenever the stream is started.
 *
 * The streams needs to have a start_with(receiver<value_type>) CPO. The receiver passed in must
 * match the value type of the stream.
 *
 * Constraints enforced on the type S modeling our concept:
 * - is copy constructable
 * - has an inner type `value_type` (representing the type of the yielded objects)
 * - CPO call start_with(S, Recv) is valid, where Recv is a receiver type with appropriate value
 * type
 *
 * Example of a stream type:
 * @code{.cpp}
 *      struct fire_once_stream {
 *          using value_type = int;
 *
 *          explicit fire_once_stream(int val): val_(val) {}
 *
 *          template <typename Recv>
 *          void start_with(Recv recv) {
 *              concore::set_value(recv, val_);
 *              concore::set_done((Recv&&) recv);
 *          }
 *      private:
 *          int val_;
 *      };
 * @endcode
 *
 * @see start_with, computation, receiver_of
 */
template <typename S>
CONCORE_CONCEPT_OR_BOOL(stream) = detail::stream_impl<S>;

//! Type traits that extracts the value type from a stream. Returns an error type if the given type
//! is not a stream.
using detail::stream_value_type;

} // namespace v1
} // namespace rxs
} // namespace concore