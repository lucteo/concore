#pragma once

#include "concore/task.hpp"
#include "concore/profiling.hpp"
#include "concore/data/detail/nodes.hpp"
#include "concore/low_level/spin_mutex.hpp"

#include <atomic>
#include <cassert>
#include <mutex>

namespace concore {

namespace detail {

/**
 * @brief      A list of tasks associated with a worker.
 *
 * For regular operations on the same worker, this looks like a stack: LIFO order.
 * For other workers stealing tasks, we steal from the bottom of the stack: FIFO order.
 *
 * The main idea is to improve locality for worker threads:
 *      - the current worker should pick up the last task created (more locale to current work)
 *      - another worker should steal the further away task possible
 *
 * Implemented as a synchronized double-linked list.
 * Using a node_factory to cache the nodes storage.
 */
class worker_tasks {
public:
    worker_tasks() {
        root_.next_.store(&root_, std::memory_order_relaxed);
        root_.prev_.store(&root_, std::memory_order_relaxed);
    }
    worker_tasks(const worker_tasks&) = delete;
    const worker_tasks& operator=(const worker_tasks&) = delete;

    //! Pushes a task on the top of the stack
    //! Cannot be called in parallel with try_pop(), but can be called in parallel with try_steal()
    void push(task&& t) {
        // Get a new node and construct the task in it
        auto node = static_cast<bidir_node_ptr>(factory_.acquire());
        construct_in_bidir_node(node, std::forward<task>(t));

        // Insert the task in the front of the list
        std::lock_guard<spin_mutex> lock{access_bottleneck_};
        auto cur_first = static_cast<bidir_node_ptr>(root_.next_.load(std::memory_order_relaxed));
        node->next_.store(cur_first, std::memory_order_relaxed);
        node->prev_.store(&root_, std::memory_order_relaxed);
        cur_first->prev_.store(node, std::memory_order_relaxed);
        root_.next_.store(node, std::memory_order_relaxed);
    }

    //! Pops one tasks from the top of the stack
    //! Cannot be called in parallel with push(), but can be called in parallel with try_steal()
    bool try_pop(task& t) {
        node_ptr first;
        // Synchronized access: get the first element from the list
        {
            std::lock_guard<spin_mutex> lock{access_bottleneck_};
            first = root_.next_.load(std::memory_order_relaxed);
            if (first != &root_) {
                auto second =
                        static_cast<bidir_node_ptr>(first->next_.load(std::memory_order_relaxed));
                root_.next_.store(second, std::memory_order_relaxed);
                second->prev_.store(&root_, std::memory_order_relaxed);
            }
        }

        if (first != &root_) {
            // Get the task out of the node and release the node
            extract_from_bidir_node(first, t);
            factory_.release(first);
            return true;
        } else
            return false;
    }

    //! Steal one task from the bottom of the stack.
    //! We steal from the bottom, to take the task with lowest locality.
    //! Can be run in parallel with push() and try_pop().
    bool try_steal(task& t) {
        CONCORE_PROFILING_FUNCTION();

        bidir_node_ptr last;
        // Synchronized access: get the last element from the list
        {
            std::lock_guard<spin_mutex> lock{access_bottleneck_};
            last = static_cast<bidir_node_ptr>(root_.prev_.load(std::memory_order_relaxed));
            if (last != &root_) {
                node_ptr prev_to_last = last->prev_.load(std::memory_order_relaxed);
                root_.prev_.store(prev_to_last, std::memory_order_relaxed);
                prev_to_last->next_.store(&root_, std::memory_order_relaxed);
            }
        }

        if (last != &root_) {
            // Get the task out of the node and release the node
            extract_from_bidir_node(last, t);
            factory_.release(last);
            return true;
        } else
            return false;
    }

private:
    //! The root node representing the double-linked list of nodes
    //! First elem == top of the stack, last elem == bottom of the stack
    detail::bidir_node_base root_;

    //! Bottleneck for synchronizing the access to the double-linked list
    spin_mutex access_bottleneck_;

    //! Object that creates nodes, and keeps track of the freed nodes.
    node_factory<task, detail::bidir_node_base> factory_;
};

} // namespace detail
} // namespace concore