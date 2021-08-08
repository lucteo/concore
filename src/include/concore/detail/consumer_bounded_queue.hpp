#pragma once

#include "concore/data/concurrent_queue.hpp"

namespace concore {

namespace detail {
/**
 * @brief      A queue used to allow processing a limited amount of items.
 *
 * This similar to @ref n_serializer, but can be used to implement other types of concurrent
 * structures. Given a limit on how many items we can actively process at once, this class helps
 * keep track when items can become active and when it's the best time to execute them.
 *
 * When adding a new item, one of the two can happen:
 * a) we haven't reached our concurrency limit, so we can start processing an item
 * b) we reached the limit, and the item needs to be added to a waiting list
 *
 * Whenever an item is considered released (done processing it), if there are non-active queued
 * items, then we can continue to process another item.
 *
 * @tparam     T     The types of items we are keeping track of
 *
 * @see        n_serializer, pipeline
 */
template <typename T>
struct consumer_bounded_queue {
    /**
     * @brief      Construct the consume bounded queue with the maximum allowed concurrency for
     *             processing the items
     *
     * @param      max_active  The maximum active items
     */
    explicit consumer_bounded_queue(int max_active)
        : max_active_(max_active) {}

    /**
     * @brief      Pushes a new item and try acquire the processing of an item.
     *
     * @param      elem  The element to be pushed to our structure
     *
     * @return     True if we can start processing a new item
     *
     * This will add the given element to the list of items that need to be processed. If there
     * are active slots, this will return true indicating that it's safe to process one item.
     *
     * The item to be processed is NOT the item that is just pushed here. Instead, the user must
     * call @ref extract_one() to get the item to be processed. The elements to be processed
     * would be extracted in the order in which they were pushed; we push to the back, and
     * extract from the front.
     *
     * @warning    If this method returns true, the user should have exactly one call to
     *             @ref extract_one(), and then exactly one call to @ref release_and_acquire()
     *
     */
    bool push_and_try_acquire(T&& elem) {
        waiting_.push(std::move(elem));

        // Increase the number of items we keep track of
        // Check if we can increase the "active" items; if so, acquire an item
        count_bits old{}, desired{};
        old.int_value = combined_count_.load();
        do {
            desired.int_value = old.int_value;
            desired.fields.total++;
            if (desired.fields.active < max_active_)
                desired.fields.active++;
        } while (!combined_count_.compare_exchange_weak(old.int_value, desired.int_value));

        return desired.fields.active != old.fields.active;
    }

    /**
     * @brief      Extracts one item to be processed
     *
     * @return     The item to be processed
     *
     * This should be called each time @ref push_and_try_acquire() or @ref release_and_acquire()
     * returns true. For each call to this function, one need to call @ref
     * release_and_acquire().
     */
    T extract_one() {
        // In the vast majority of cases there is a task ready to be executed; but, just in case,
        // we spin if we fail to extract a task
        T elem;
        spin_backoff spinner;
        while (!waiting_.try_pop(elem))
            spinner.pause();
        return elem;
    }

    /**
     * @brief      Called when an item is done processing
     *
     * @return     True if the caller needs to start processing another item
     *
     * This should be called each time we are done processing an item (got with @ref
     * extract_one()). If this returns true, it means that we should immediately start
     * processing another item. For this one needs to call @ref extract_one() and then @ref
     * release_and_acquire() again.
     */
    bool release_and_acquire() {
        // Decrement the number of elements
        // Check if we need to acquire a new element
        count_bits old{}, desired{};
        old.int_value = combined_count_.load();
        do {
            desired.int_value = old.int_value;
            desired.fields.total--;
            desired.fields.active = std::min(desired.fields.active, desired.fields.total);
        } while (!combined_count_.compare_exchange_weak(old.int_value, desired.int_value));

        // If the number of active items remains the same, it means that we just acquired a slot
        return desired.fields.active == old.fields.active;
    }

private:
    //! The maximum number of active items
    uint32_t max_active_;
    //! Queue of items that are waiting (or not yet extracted)
    concurrent_queue<T> waiting_;
    //! The count of items in our queue (active & total)
    std::atomic<uint64_t> combined_count_{0};

    union count_bits {
        uint64_t int_value;
        struct {
            uint32_t active;
            uint32_t total;
        } fields;
    };
};

} // namespace detail
} // namespace concore
