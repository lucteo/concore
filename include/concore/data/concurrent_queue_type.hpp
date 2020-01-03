#pragma once

namespace concore {
inline namespace v1 {

//! Defines the queue type, based o the desired level of concurrency for producers and consumers.
enum class queue_type {
    single_prod_single_cons,
    single_prod_multi_cons,
    multi_prod_single_cons,
    multi_prod_multi_cons,

    default_type = multi_prod_multi_cons
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