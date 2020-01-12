#pragma once

#include <atomic>
#include <mutex>
#include <vector>

namespace concore {
namespace detail {

struct node_base;
using node_ptr = node_base*;

//! The base implementation of a singly-linked list node; uses type erasure to save code size.
struct node_base {
    //! Link to the next node in the list
    std::atomic<node_ptr> next_;
};

//! A node with data; this will be used for operations that involve the actual data of the node.
template <typename T>
struct node : node_base {
    //! The data of the node
    T value_;
};

//! Base class for a node factory.
//! This implements the core acquire() and release() logic, and keeps track of the free list.
//! It does not allocate memory. This is type-agnostic.
class node_factory_base {
protected:
    node_factory_base() = default;
    ~node_factory_base() = default;

    //! Acquire a new node from the free list. This is typically very fast
    //! Returns null, if we don't have any nodes in our free list
    node_ptr acquire() {
        node_ptr old_head = free_list_.load(std::memory_order_acquire);
        while (true) {
            if (!old_head)
                return nullptr;

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

    //! Called when the factory is empty with a new array of nodes to be used
    void use_nodes(node_ptr array, int stride, int count) {
        // Set up the links for the new nodes
        for (int i = 1; i < count; i++)
            array_at(array, stride, i - 1)
                    ->next_.store(array_at(array, stride, i), std::memory_order_relaxed);
        node_ptr last = array_at(array, stride, count - 1);

        // Add the new nodes to our free list
        node_ptr old_head = free_list_.load(std::memory_order_acquire);
        while (true) {
            last->next_.store(old_head, std::memory_order_relaxed);
            if (free_list_.compare_exchange_weak(old_head, array, std::memory_order_acq_rel))
                break;
        }
    }

private:
    //! The list (singly-linked) of free nodes
    std::atomic<node_ptr> free_list_{nullptr};

    //! Index in the array of nodes, given the stride
    static node_ptr array_at(node_ptr arr, int stride, int idx) {
        return reinterpret_cast<node_ptr>(reinterpret_cast<char*>(arr) + stride * idx);
    }
};

//! A typed node factory.
//! This will know to construct node<T> objects, with the allocator given in constructor.
//! Instead of freeing the released nodes, it will chain them to a free list, for later reuse.
//!
//! Implemented in terms of node_factory_base, which is type-agnostic.
template <typename T, typename A = std::allocator<T>>
class node_factory : node_factory_base {
public:
    using allocator_type = A;

    explicit node_factory(int num_nodes_per_chunk = 1024, allocator_type a = {})
        : num_nodes_per_chunk_(num_nodes_per_chunk)
        , allocator_(std::move(a)) {
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
        while (true) {
            // Try to get a free node from the base
            node_ptr node = node_factory_base::acquire();
            if (node)
                return node;

            // No free nodes? Allocate some more
            allocate_nodes();
        }
    }

    //! Releases a node; chain it to our free list. No memory is freed at this point
    using node_factory_base::release;

private:
    //! How many nodes do we allocate at once, in a chunk
    int num_nodes_per_chunk_;
    //! The allocator used to allocate memory
    allocator_type allocator_;

    //! All the chunks that we've allocated so far
    std::vector<node_ptr> allocated_chunks_;
    //! Used to protect our vector of chunks
    std::mutex bottleneck_;

    //! Allocates one chunk of memory, chain the nodes, and add them to our free list
    void allocate_nodes() {
        // Allocate memory for the new chunk
        typename decltype(allocator_)::template rebind<node<T>>::other node_allocator;
        node_ptr nodes_array = node_allocator.allocate(num_nodes_per_chunk_);

        // Pass it to base to populate the free list
        node_factory_base::use_nodes(nodes_array, sizeof(node<T>), num_nodes_per_chunk_);

        // Add this to our vector of allocated chunks,
        std::lock_guard<std::mutex> lock{bottleneck_};
        allocated_chunks_.push_back(nodes_array);
    }
};

//! Construct the value inside a node by moving in the given value.
template <typename T>
void construct_in_node(node_ptr n, T&& value) {
    node<T>* typed_node = static_cast<node<T>*>(n);
    new (&typed_node->value_) T(std::forward<T>(value));
}

//! Extract the value stored in the node into the passed param, and destroys the value in the node.
template <typename T>
void extract_from_node(node_ptr n, T& value) {
    node<T>* typed_node = static_cast<node<T>*>(n);
    value = std::move(typed_node->value_);
    typed_node->value_.~T();
}

} // namespace detail
} // namespace concore