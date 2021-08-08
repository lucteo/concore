/**
 * @file    concurrent_queue.hpp
 * @brief   Definition of @ref concore::v1::concurrent_queue "concurrent_queue"
 *
 * @see     @ref concore::v1::concurrent_queue "concurrent_queue"
 */
#pragma once

#include "concurrent_dequeue.hpp"

namespace concore {

inline namespace v1 {

/**
 * @brief      Concurrent queue implementation
 *
 * @tparam     T         The type of elements to store
 *
 * The queue, has 2 ends:
 *  - the *back*: where new element can be added
 *  - the *front*: from which elements can be extracted
 *
 * The queue has only 2 operations corresponding to pushing new elements into the queue and popping
 * elements out of the queue: @ref push() and @ref try_pop().
 *
 * There can be any number of threads calling @ref push() and @ref pop().
 *
 * @warning: The move constructor of the given type must not throw.
 *
 * Exceptions guarantees:
 * - push might throw while allocating memory; in this case, the element is not added
 *
 * Thread safety: except the following methods, everything else can be used concurrently:
 * - constructors
 * - copy/move assignments
 * - @ref unsafe_clear()
 *
 * This queue does not provide any iterators, as those would be thread unsafe.
 *
 * @see push(), try_pop(), concurrent_dequeue
 */
template <typename T>
class concurrent_queue {
public:
    //! The value type of the concurrent queue.
    using value_type = T;

    //! Default constructor. Creates a valid empty queue.
    concurrent_queue() = default;
    /**
     * @brief      Constructs a new instance of the queue, with the given preallocated size.
     *
     * @param      expected_size  How many elements to preallocate in our fast queue.
     *
     * If we ever add more elements in our queue than the given limit, our queue starts to become
     * slower.
     *
     * The number of reserved elements should be bigger than the expected concurrency.
     */
    explicit concurrent_queue(size_t expected_size)
        : data_(expected_size) {}

    //! Destructor
    ~concurrent_queue() = default;

    //! Copy constructor
    concurrent_queue(const concurrent_queue&) = default;
    //! Copy assignment
    concurrent_queue& operator=(const concurrent_queue&) = default;

    //! Move constructor
    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    concurrent_queue(concurrent_queue&&) = default;
    //! Move assignment
    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    concurrent_queue& operator=(concurrent_queue&&) = default;

    /**
     * @brief      Pushes one element in the back of the queue.
     *
     * @param      elem  The element to be added to the queue
     *
     * @details
     *
     * This operation is thread-safe.
     *
     * @see try_pop()
     */
    void push(T&& elem) { data_.push_back(std::move(elem)); }

    /**
     * @brief      Try to pop one element from the front of the queue
     *
     * @param      elem  [out] Location where to put the popped element
     *
     * @return     True if an element was popped; false otherwise.
     *
     * @details
     *
     * If the queue is empty, this will return false and not touch the given parameter.
     * If the queue is not empty, it will extract the element from the front of the queue and store
     * it in the given parameter.
     *
     * This operation is thread-safe.
     *
     * @see push()
     */
    bool try_pop(T& elem) noexcept { return data_.try_pop_front(elem); }

    //! Clears the content of the queue.
    //! This is not thread safe.
    void unsafe_clear() noexcept { return data_.unsafe_clear(); }

private:
    //! This is implemented in terms of a concurrent dequeue
    concurrent_dequeue<T> data_;
};

} // namespace v1
} // namespace concore