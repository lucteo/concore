#pragma once

#include "spin_backoff.hpp"

#include <atomic>
#include <vector>
#include <deque>
#include <mutex>
#include <cstdlib>

namespace concore {

namespace detail {

//! A node; we use chained lists to implement the queue
template <typename T>
struct node {
    std::atomic<node*> next_;
    T value_;
};

// TODO: use the _base trick to reduce the code size generated by the template.

//! Object used to allocate nodes, and keep track of nodes that are not needed anymore.
//! This will never release memory, until destructor. The allocated nodes are reused.
template <typename T>
class node_factory {
public:
    using node_ptr = node<T>*;

    node_factory() {
        // Ensure we are not contending here when we start our operations
        allocate_nodes();
    }
    ~node_factory() {
        // Free the allocated chunks
        for (node_ptr chunk : allocated_chunks_)
            std::free(chunk);
    }

    //! Acquire a new node from the factory. This is typically very fast
    node_ptr acquire() {
        node_ptr old_head = free_list_.load(std::memory_order_acquire);
        while (true) {
            // Ensure that we have at least one node
            while (!old_head) {
                allocate_nodes();
                old_head = free_list_.load(std::memory_order_acquire);
            }

            // Nobody changes the links in the free list, so we don't need to atomically load the
            // next_ pointer.
            // If, somehow the node is extracted and the next_ is changed in the meantime, then our
            // CAS will fail anyway.
            node_ptr new_head = old_head->next_.load(std::memory_order_relaxed);

            // Now try to do the exchange
            if (free_list_.compare_exchange_weak(old_head, new_head, std::memory_order_acq_rel))
                break;
        }
        return old_head;
    }

    //! Releases a node; chain it to our free list. No memory is freed at this point
    void release(node_ptr node) {
        node_ptr old_head = free_list_.load(std::memory_order_acquire);
        while (true) {
            // We have ownership of the given node, so we can change the next_ pointer relaxed
            node->next_.store(old_head, std::memory_order_relaxed);
            if (free_list_.compare_exchange_weak(old_head, node, std::memory_order_acq_rel))
                break;
        }
    }

private:
    //! The number of elements in a chunk
    static constexpr int chunk_size_ = 4096;
    //! The list (singly-linked) of free nodes
    std::atomic<node_ptr> free_list_{nullptr};
    //! All the chunks that we've allocated so far
    std::vector<node_ptr> allocated_chunks_;
    //! Used to protect our vector of chunks
    std::mutex bottleneck_;

    //! Allocates one chunk of memory, chain the nodes, and add them to our free list
    void allocate_nodes() {
        // Allocate memory for the new chunk
        node_ptr nodes_array =
                reinterpret_cast<node_ptr>(std::calloc(chunk_size_, sizeof(node<T>)));
        // Set up the links for the new nodes
        for (int i = 1; i < chunk_size_; i++)
            nodes_array[i - 1].next_.store(&nodes_array[i], std::memory_order_relaxed);

        // Add the new nodes to our free list
        node_ptr old_head = free_list_.load(std::memory_order_acquire);
        while (true) {
            nodes_array[chunk_size_ - 1].next_.store(old_head, std::memory_order_relaxed);
            if (free_list_.compare_exchange_weak(old_head, nodes_array, std::memory_order_acq_rel))
                break;
        }

        // Add this to our vector of allocated chunks,
        std::lock_guard<std::mutex> lock{bottleneck_};
        allocated_chunks_.push_back(nodes_array);
    }
};

} // namespace detail

inline namespace v1 {

/**
 * @brief      Concurrent single-ended queue implementation
 *
 * @tparam     T     The type of elements to store
 *
 * This is a multi-producer, single-consumer queue. That is, multiple threads can call push() in
 * parallel, but only one thread should be able to call pop().
 */
template <typename T>
class concurrent_queue {
public:
    using value_type = T;

    // concurrent_queue() = default;
    concurrent_queue() {
        last_link_.store(&head_, std::memory_order_relaxed);
    }

    //! Pushes one element in the back of the queue.
    //! Multiple produces can call this in parallel.
    void push(T&& elem) {
        // Fill up a new node; use in-place move ctor
        node_ptr node = factory_.acquire();
        new (&node->value_) T(std::forward<T>(elem));
        node->next_.store(nullptr, std::memory_order_relaxed);

        // Chain the element at the end of the queue
        std::atomic<node_ptr>* old_link = last_link_.exchange(&node->next_);
        // Note: there can be a short gap between chaining elements to the end, and actually being
        // able to access them while iterating from the start.
        old_link->store(node);
    }

    //! Try to pop one element from the front of the queue. Returns false if the queue is empty.
    //! This is considered the default popping operation.
    bool try_pop(T& elem) {
        // Easy check for empty queue
        node_ptr current = head_.load(std::memory_order_relaxed);
        if (!current)
            return false;

        // Check if the queue has more than one element
        node_ptr second = current->next_.load(std::memory_order_acquire);
        if (second) {
            // If we have a second element, there is no contention on the head. Easy peasy.
            // The head now becomes the second element
            head_.store(second, std::memory_order_relaxed);
        } else {
            // Try to set the head to null, but consider a producer that just pushes an element.
            head_.store(nullptr, std::memory_order_release);
            auto old = &current->next_;
            if (last_link_.compare_exchange_strong(old, &head_)) {
                // No contention, we've made the transition to an empty queue.
                assert(last_link_.load(std::memory_order_relaxed) != nullptr);
            } else {
                // A producer just updated last_link_ (adding a new element after 'current')
                // Wait but current->next_ to be filled, and use this as head.
                spin_backoff spinner;
                while (true) {
                    second = current->next_.load(std::memory_order_acquire);
                    if (second)
                        break;
                    spinner.pause();
                }
                // The newly added element is the new head
                head_.store(second);
                // Note, that after adding a head element, further pushes cannot change the head.
            }
        }

        // We just popped the head of the queue. Get it's content, and release the node
        elem = std::move(current->value_);
        current->value_.~T();
        factory_.release(current);
        return true;
    }

private:
    using node_ptr = detail::node<T>*;

    //! Object that creates nodes, and keeps track of the freed nodes.
    detail::node_factory<T> factory_;

    //! The first element in our queue
    std::atomic<node_ptr> head_{nullptr};
    //! The next_ for the last element in our queue.
    //! In general, the link will point to null, but this pointer will never be null.
    //! When the queue is empty, this will store the address of 'head_'.
    std::atomic<std::atomic<node_ptr>*> last_link_{&head_};
};

} // namespace v1
} // namespace concore