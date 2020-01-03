#pragma once

#include "nodes.hpp"
#include "../../low_level/spin_backoff.hpp"

#include <atomic>

namespace concore {

namespace detail {

//! Base data for a concurrent_queue implementation
struct concurrent_queue_data {
    //! The first element in our queue
    std::atomic<node_ptr> head_{nullptr};
    //! The next_ for the last element in our queue.
    //! In general, the link will point to null, but this pointer will never be null.
    //! When the queue is empty, this will store the address of 'head_'.
    std::atomic<std::atomic<node_ptr>*> last_link_{&head_};
};

//! Pushes an already constructed node on the back of the queue
void push_back(concurrent_queue_data& queue, node_ptr node) {
    // There is nothing after the given node
    node->next_.store(nullptr, std::memory_order_relaxed);

    // Chain the element at the end of the queue
    std::atomic<node_ptr>* old_link = queue.last_link_.exchange(&node->next_);
    // Note: there can be a short gap between chaining elements to the end, and actually being
    // able to access them while iterating from the start.
    old_link->store(node);
}

//! Tries to pop one element from the front of a concurrent_queue that allows only one consumer at
//! the front of the queue, and no producers at the front.
node_ptr try_pop_front_single(concurrent_queue_data& queue) {
    // Easy check for empty queue
    node_ptr head = queue.head_.load(std::memory_order_relaxed);
    if (!head)
        return nullptr;

    // Check if the queue has more than one element
    node_ptr second = head->next_.load(std::memory_order_acquire);
    if (second) {
        // If we have a second element, there is no contention on the head. Easy peasy.
        // The head now becomes the second element
        queue.head_.store(second, std::memory_order_relaxed);
    } else {
        // Try to set the head to null, but consider a producer that just pushes an element.
        queue.head_.store(nullptr, std::memory_order_release);
        auto old = &head->next_;
        if (queue.last_link_.compare_exchange_strong(old, &queue.head_)) {
            // No contention, we've made the transition to an empty queue.
            assert(queue.last_link_.load(std::memory_order_relaxed) != nullptr);
        } else {
            // A producer just updated queue.last_link_ (adding a new element after 'head')
            // Wait but head->next_ to be filled, and use this as head.
            spin_backoff spinner;
            while (true) {
                second = head->next_.load(std::memory_order_acquire);
                if (second)
                    break;
                spinner.pause();
            }
            // The newly added element is the new head
            queue.head_.store(second);
            // Note, that after adding a head element, further pushes cannot change the head.
        }
    }
    return head;
}

//! Same as try_pop_front_single(), but allow multiple consumers on the front of the queue
node_ptr try_pop_front_multi(concurrent_queue_data& queue) {
    // Easy check for empty queue
    node_ptr head = queue.head_.load(std::memory_order_acquire);
    if (!head)
        return nullptr;

    // Try to exchange the head with the next element.
    // Producers and consumers might try to operate on head_
    node_ptr next;
    while (true) {
        if (!head)
            return nullptr;

        next = head->next_.load();
        if (queue.head_.compare_exchange_weak(head, next))
            break;
    }

    // If we just consume the last element from the queue, we need to update the last link
    if (next == nullptr) {
        auto old = &head->next_;
        if (queue.last_link_.compare_exchange_strong(old, &queue.head_)) {
            // No contention, we've made the transition to an empty queue.
            assert(queue.last_link_.load(std::memory_order_relaxed) != nullptr);
        } else {
            // A producer just updated queue.last_link_ (adding a new element after 'head')
            // Wait but head->next_ to be filled, and use this as head.
            spin_backoff spinner;
            node_ptr new_head;
            while (true) {
                new_head = head->next_.load(std::memory_order_acquire);
                if (new_head)
                    break;
                spinner.pause();
            }
            assert(queue.head_.load(std::memory_order_relaxed) == nullptr);
            // The newly added element is the new head
            queue.head_.store(new_head);
            // Note 1: after adding a head element, further pushes cannot change the head.
            // Note 2: before this last store, all parallel pops will fail as the head is null.
        }
    }

    // Return the element that we've popped from the front of the queue
    return head;
}

} // namespace detail
} // namespace concore