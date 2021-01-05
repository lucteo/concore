/**
 * @file    concurrent_queue_type.hpp
 * @brief   Definition of queue_type
 *
 * @see     queue_type
 */
#pragma once

namespace concore {
inline namespace v1 {

/**
 * @brief      Queue type, based o the desired level of concurrency for producers and consumers.
 *
 * Please note that this express only the desired type. It doesn't mean that implementation will be
 * strictly obey the policy. The implementation can be more conservative and fall-back to less
 * optimal implementation. For example, the implementation can always use the
 * multi_prod_multi_cons type, as it includes all the constraints for all the other types.
 */
enum class queue_type {
    single_prod_single_cons, //!< Single-producer, single-consumer concurrent queue.
    single_prod_multi_cons,  //!< Single-producer, multiple-consumer concurrent queue.
    multi_prod_single_cons,  //!< Multiple-producer, single-consumer concurrent queue.
    multi_prod_multi_cons,   //!< Multiple-producer, multiple-consumer concurrent queue.

    default_type = multi_prod_multi_cons //!< The default queue type. Multiple-producer,
                                         //!< multiple-consumer concurrent queue.
};

} // namespace v1

namespace detail {
//! Check if the queue type is a single-consumer one
constexpr bool is_single_consumer(queue_type t) {
    return t == queue_type::single_prod_single_cons || t == queue_type::multi_prod_single_cons;
}
//! Check if the queue type is a single-producer one
constexpr bool is_single_producer(queue_type t) {
    return t == queue_type::single_prod_single_cons || t == queue_type::single_prod_multi_cons;
}
} // namespace detail

} // namespace concore