#pragma once

#include "../task.hpp"
#include "../profiling.hpp"
#include "../data/detail/nodes.hpp"
#include "../low_level/spin_backoff.hpp"

#include <atomic>
#include <cassert>

namespace concore {

namespace detail {

//! A stack for holding the tasks of a worker, that can be used to steal tasks from the bottom.
class worker_tasks {
public:
    worker_tasks() = default;
    worker_tasks(const worker_tasks&) = delete;
    const worker_tasks& operator=(const worker_tasks&) = delete;

    //! Pushes a task on the top of the stack
    //! Cannot be called in parallel with try_pop()
    void push(task&& t) {
        // Get a new node and construct the task in it
        node_ptr node = factory_.acquire();
        construct_in_node(node, std::forward<task>(t));

        // Push the new node on top of the stack
        node_ptr cur_top = lock_aquire(top_);
        node->next_.store(cur_top, std::memory_order_release);
        // At this point, 'cur_top' is either a locked node, or null. That means that try_steal
        // cannot make any changes to it. We can safely replace it with our new head.
        // When doing this, we also release the lock.
        top_.store(node, std::memory_order_release);
    }

    //! Pops one tasks from the top of the stack
    //! Cannot be called in parallel with push(), but can be called in parallel with try_steal()
    bool try_pop(task& t) {
        // Lock the top node, so that we can't steal it.
        node_ptr cur_top = lock_aquire(top_);
        if (cur_top) {
            // Now, lock the link to the second node, making sure there is no stealer that is still
            // looking at the second node
            node_ptr second = lock_aquire(cur_top->next_);
            // Ok. Nobody is looking at the first and second nodes; we can safely change the links
            top_.store(second, std::memory_order_release);
            lock_release(cur_top->next_);

            // Get the task our of the node and release the node
            extract_from_node(cur_top, t);
            factory_.release(cur_top);
            return true;
        }
        // No node to be popped
        return false;
    }

    //! Steal one task from the bottom of the stack.
    //! We steal from the bottom, to take the task with lowest locality.
    //! Can be run in parallel with push() and try_pop().
    bool try_steal(task& t) {
        CONCORE_PROFILING_FUNCTION();
        // Lock the top while trying to access the list
        std::atomic<node_ptr>* link_to_node = &top_;
        node_ptr cur = lock_aquire(*link_to_node);
        if (!cur)
            return false;

        // If we have more elements, traverse the stack, while locking each element in turn
        while (cur->next_.load(std::memory_order_relaxed)) {
            auto next = lock_aquire(cur->next_, std::memory_order_relaxed);
            lock_release(*link_to_node);
            link_to_node = &cur->next_;
            cur = next;
        }

        // At this point, 'cur' is the last element, and link_to_node is the .next_ link that points
        // to it. link_to_node is locked.
        node_ptr stolen_node = cur;
        link_to_node->store(nullptr, std::memory_order_release);
        // this will also clear the lock bit

        // Extract the task from the node
        extract_from_node(stolen_node, t);
        factory_.release(stolen_node);
        return true;
    }

private:
    //! The top of the stack
    std::atomic<node_ptr> top_{nullptr};

    //! Object that creates nodes, and keeps track of the freed nodes.
    node_factory<task> factory_;

    //! Acquire the lock over a node, and return the node pointer (without the lock bit)
    //! Spins if the node is already locked.
    //! Doesn't do any locking if the node pointer is null
    node_ptr lock_aquire(
            std::atomic<node_ptr>& aptr, std::memory_order order = std::memory_order_acquire) {
        spin_backoff spinner;
        node_ptr node = aptr.load(std::memory_order_acquire);
        while (node) {
            node = clear_lock_bit(node);
            node_ptr locked_node = set_lock_bit(node);
            if (aptr.compare_exchange_weak(node, locked_node))
                break;
            spinner.pause();
        }
        return node;
    }

    //! Release the lock we had on a node pointer
    void lock_release(std::atomic<node_ptr>& aptr) {
        node_ptr node = aptr.load(std::memory_order_relaxed);
        if (node) {
            assert(is_locked(node));
            aptr.store(clear_lock_bit(node), std::memory_order_release);
        }
    }

    static constexpr uintptr_t one = 1;

    //! Sets the lock bit to the given pointer.
    node_ptr set_lock_bit(node_ptr p) {
        auto as_num = reinterpret_cast<uintptr_t>(p);
        return reinterpret_cast<node_ptr>(as_num | one);
    }

    //! Check if the pointer is locked or not
    bool is_locked(node_ptr p) { return (reinterpret_cast<uintptr_t>(p) & one) != 0; }

    //! Clear the lock bit from a pointer
    node_ptr clear_lock_bit(node_ptr p) {
        auto as_num = reinterpret_cast<uintptr_t>(p);
        return reinterpret_cast<node_ptr>(as_num & ~one);
    }
};

} // namespace detail
} // namespace concore