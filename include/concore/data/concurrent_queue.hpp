#pragma once

#include "concurrent_queue_type.hpp"
#include "detail/nodes.hpp"
#include "detail/concurrent_queue_impl.hpp"
#include "../low_level/spin_backoff.hpp"

#include <atomic>

namespace concore {

inline namespace v1 {

/**
 * @brief      Concurrent single-ended queue implementation
 *
 * @tparam     T         The type of elements to store
 * @tparam     conc_type The expected concurrency for the queue
 *
 * Based on the conc_type parameter, this can be:
 *  - single-producer, single-consumer
 *  - single-producer, multi-consumer
 *  - multi-producer, single-consumer
 *  - multi-producer, multi-consumer
 *
 * Note, that the implementation for some of these alternatives might coincide.
 */
template <typename T, queue_type conc_type = queue_type::multi_prod_multi_cons>
class concurrent_queue {
public:
    using value_type = T;

    concurrent_queue() = default;
    concurrent_queue(const concurrent_queue&) = delete;
    const concurrent_queue& operator=(const concurrent_queue&) = delete;

    //! Pushes one element in the back of the queue.
    void push(T&& elem) {
        // Fill up a new node; use in-place move ctor
        node_ptr node = factory_.acquire();
        detail::construct_in_node(node, std::forward<T>(elem));
        detail::push_back(queue_, node);
    }

    //! Try to pop one element from the front of the queue. Returns false if the queue is empty.
    //! This is considered the default popping operation.
    bool try_pop(T& elem) {
        node_ptr node;
        if constexpr (detail::is_single_consumer(conc_type))
            node = detail::try_pop_front_single(queue_);
        else {
            node = detail::try_pop_front_multi(queue_);
        }
        if (node) {
            detail::extract_from_node(node, elem);
            factory_.release(node);
            return true;
        } else
            return false;
    }

private:
    using node_ptr = detail::node_ptr;

    //! The data holding the actual queue
    detail::concurrent_queue_data queue_;

    //! Object that creates nodes, and keeps track of the freed nodes.
    detail::node_factory<T> factory_;
};

} // namespace v1
} // namespace concore