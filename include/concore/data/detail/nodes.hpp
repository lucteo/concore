#pragma once

#include <atomic>
#include <memory>

namespace concore {
namespace detail {

struct node_base;
struct bidir_node_base;

//! A pointer to a node. Can be a single-list node or a doubly-lined node.
using node_ptr = node_base*;

//! A pointer to a bidirectional node. Used in doubly-lined node.
using bidir_node_ptr = bidir_node_base*;

//! The base implementation of a singly-linked list node; uses type erasure to save code size.
//! A double-linked list will also derive from this class.
struct node_base {
    //! Link to the next node in the list
    std::atomic<node_ptr> next_;
};

//! Base implementation for a double-linked list node; uses type erasure to save code size.
struct bidir_node_base : node_base {
    //! Link to the previous node in the list
    std::atomic<node_ptr> prev_;
};

//! A node with data; this will be used for operations that involve the actual data of the node.
//! This can be used for a single-lined list or for a double-linked list
template <typename T, typename Base = node_base>
struct node : Base {
    //! The data of the node
    T value_;
};

/**
 * @brief      A free list of nodes
 *
 * Independent of the actual type of nodes, for reusing generated code.
 *
 * User can call @ref acquire() to get one element from the free list, and @ref release() to add
 * an element to the free file. Allocated elements can be added with @ref use_nodes() method.
 *
 * This can be accessed from multiple threads. For example one thread may acquire a node while
 * another thread releases a node.
 *
 * This does not allocate memory, as it's node type-agnostic.
 * Can be used both for singly-linked and double-linked lists.
 */
class node_free_list {
public:
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
        // NOLINTNEXTLINE
        return reinterpret_cast<node_ptr>(reinterpret_cast<char*>(arr) + stride * idx);
    }
};

/**
 * @brief      Factory of node objects
 *
 * @tparam     T     The content of a node object
 * @tparam     B     The base class for the node; either a singly-linked or double-linked node
 * @tparam     A     The allocator used to allocate nodes
 *
 * This will try to cache allocated nodes, so that we reduce the amount of allocations needed.
 * Whenever new nodes are needed, this will allocate a chunk of nodes, and add them to the internal
 * free list. If there are freed nodes, it will returned those nodes.
 *
 * Memory allocated will only be released at the end.
 */
template <typename T, typename B = node_base, typename A = std::allocator<T>>
class node_factory {
public:
    using allocator_type = A;
    using node_type = node<T, B>;

    explicit node_factory(int num_nodes_per_chunk = 1024, allocator_type a = {})
        : num_nodes_per_chunk_(num_nodes_per_chunk)
        , allocator_(std::move(a)) {
        // Ensure we are not contending here when we start our operations
        allocate_nodes();
    }

    ~node_factory() {
        // Free the allocated chunks
        node_ptr chunk = allocated_chunks_;
        while (chunk) {
            node_ptr next = chunk->next_;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
            dealloc(static_cast<node_of_node*>(chunk)->value_, num_nodes_per_chunk_);
            dealloc(chunk, 1);
            chunk = next;
        }
    }

    node_factory(const node_factory&) = delete;
    node_factory& operator=(const node_factory&) = delete;

    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    node_factory(node_factory&&) = default;
    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    node_factory& operator=(node_factory&&) = default;

    //! Acquire a new node from the factory. This is typically very fast
    node_ptr acquire() {
        while (true) {
            // Try to get a free node from the base
            node_ptr node = free_list_.acquire();
            if (node)
                return node;

            // No free nodes? Allocate some more
            allocate_nodes();
        }
    }

    //! Releases a node; chain it to our free list. No memory is freed at this point
    void release(node_ptr node) { free_list_.release(node); }

private:
    //! How many nodes do we allocate at once, in a chunk
    int num_nodes_per_chunk_;
    //! The allocator used to allocate memory
    allocator_type allocator_;

    using node_of_node = node<node_ptr>;

    //! List (single-linked) of chunks we've allocated so far.
    std::atomic<node_ptr> allocated_chunks_{nullptr};

    //! The list with free nodes, ready to be used
    node_free_list free_list_;

    //! Allocates one chunk of memory, chain the nodes, and add them to our free list
    //! This may be called in parallel by multiple threads; in this case, we may over-allocate, but
    //! we would still use all the nodes allocated.
    void allocate_nodes() {
        // Allocate memory for the new chunk
        node_ptr nodes_array = alloc<node_type>(num_nodes_per_chunk_);

        // Pass it to base to populate the free list
        free_list_.use_nodes(nodes_array, sizeof(node_type), num_nodes_per_chunk_);

        // Add this to our list of allocated chunks (with another allocation)
        node_of_node* chunk = alloc<node_of_node>(1);
        chunk->value_ = nodes_array;

        // Atomically add the new chunk to the top of the list
        auto old_head = allocated_chunks_.load(std::memory_order_acquire);
        while (true) {
            chunk->next_ = old_head;
            if (allocated_chunks_.compare_exchange_weak(old_head, chunk, std::memory_order_acq_rel))
                break;
        }
    }

    template <typename TT>
    TT* alloc(size_t n) {
        typename std::allocator_traits<decltype(allocator_)>::template rebind_alloc<TT> a;
        return a.allocate(n);
    }

    template <typename TT>
    void dealloc(TT* p, size_t n) {
        typename std::allocator_traits<decltype(allocator_)>::template rebind_alloc<TT> a;
        a.deallocate(p, n);
    }
};

//! Construct the value inside a sigle-linked node by moving in the given value.
template <typename T>
void construct_in_node(node_ptr n, T&& value) {
    auto typed_node = static_cast<node<T>*>(n);
    new (&typed_node->value_) T(std::forward<T>(value));
}

//! Construct the value inside a double-linked node by moving in the given value.
template <typename T>
void construct_in_bidir_node(node_ptr n, T&& value) {
    auto typed_node = static_cast<node<T, bidir_node_base>*>(n);
    new (&typed_node->value_) T(std::forward<T>(value));
}

//! Extract the value stored in the node (single-linked) into the passed param, and destroys the
//! value in the node.
template <typename T>
void extract_from_node(node_ptr n, T& value) {
    auto typed_node = static_cast<node<T>*>(n);
    value = std::move(typed_node->value_);
    typed_node->value_.~T();
}

//! Extract the value stored in the node (single-linked) into the passed param, and destroys the
//! value in the node.
template <typename T>
void extract_from_bidir_node(node_ptr n, T& value) {
    auto typed_node = static_cast<node<T, bidir_node_base>*>(n);
    value = std::move(typed_node->value_);
    typed_node->value_.~T();
}

} // namespace detail
} // namespace concore