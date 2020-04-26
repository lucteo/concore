#pragma once

#include "spin_backoff.hpp"

#include <atomic>

namespace concore {
inline namespace v1 {

/**
 * @brief      A shared (read-write) mutex class that uses CPU spinning.
 *
 * For mutexes that protect very small regions of code, a shared_spin_mutex can be much faster than
 * a traditional shared_mutex. Instead of taking a lock, this will spin on the CPU, trying to avoid
 * yielding the CPU quanta.
 *
 * The ownership of the mutex can fall in 3 categories:
 *  - no ownership -- no thread is using the mutex
 *  - exclusive ownership -- only one thread can access the mutex, exclusively (*WRITE* operations)
 *  - shared ownership -- multiple threads can access the mutex in a shared way (*READ* operations)
 *
 * While one threads acquires exclusive ownership, no other thread can have shared ownership.
 * Multiple threads can have a shared ownership over the mutex.
 *
 * This implementation favors exclusive ownership versus shared ownership. If a thread is waiting
 * for exclusive ownership and one thread is waiting for the shared ownership, the thread that waits
 * on the exclusive ownership will be granted the ownership first.
 *
 * This uses an exponential backoff spinner. If after some time doing small waits it cannot enter
 * the  critical section, it will yield the CPU quanta of the current thread.
 *
 * Spin shared mutexes should only be used to protect very-small regions of code; a handful of CPU
 * instructions. For larger scopes, a traditional shared mutex may be faster; but then, think about
 * using @ref rw_serializer to avoid mutexes completely.
 *
 * @see spin_mutex, spin_backoff, rw_serializer
 */
class shared_spin_mutex {
public:
    /**
     * @brief      Default constructor.
     *
     * Constructs a shared spin mutex that is in the *no ownership* state.
     */
    shared_spin_mutex() = default;

    //! Copy constructor is DISABLED
    shared_spin_mutex(const shared_spin_mutex&) = delete;
    //! Copy assignment is DISABLED
    shared_spin_mutex& operator=(const shared_spin_mutex&) = delete;

    /**
     * @brief      Acquires exclusive ownership of the mutex
     *
     * This will put the mutex in the *exclusive ownership* case. If other threads have exclusive or
     * shared ownership, this will wait until those threads are done
     *
     * Uses a @ref spin_backoff to spin while waiting for the ownership to be free. When exiting
     * this function the mutex will be exclusively owned by the current thread.
     *
     * An @ref unlock() call must be made for each call to lock().
     *
     * @see try_lock(), unlock(), lock_shared()
     */
    void lock();
    /**
     * @brief      Tries to acquire exclusive ownership; returns false it fails the acquisition.
     *
     * @return     True if the mutex exclusive ownership was acquired; false if the mutex is busy
     *
     * This is similar to @ref lock() but does not wait for the mutex to be free again. If the mutex
     * is acquired by a different thread, or if the mutex has shared ownership this will return
     * false.
     *
     * An @ref unlock() call must be made for each call to this method that returns true.
     *
     * @see lock(), unlock()
     */
    bool try_lock();
    /**
     * @brief      Releases the exclusive ownership on the mutex
     *
     * This needs to be called for every @ref lock() and for every @ref try_lock() that returns
     * true. It should not be called without a matching @ref lock() or @ref try_lock().
     *
     * @see lock(), try_lock()
     */
    void unlock();

    /**
     * @brief      Acquires shared ownership of the mutex
     *
     * This will put the mutex in the *shared ownership* case. If other threads have exclusive
     * ownership, this will wait until those threads are done.
     *
     * Uses a @ref spin_backoff to spin while waiting for the ownership to be free. When exiting
     * this function the mutex will be exclusively owned by the current thread.
     *
     * An @ref unlock_shared() call must be made for each call to lock().
     *
     * @see try_lock_shared(), unlock_shared(), lock()
     */
    void lock_shared();
    /**
     * @brief      Tries to acquire shared ownership; returns false it fails the acquisition.
     *
     * @return     True if the mutex shared ownership was acquired; false if the mutex is busy
     *
     * This is similar to @ref lock_shared() but does not wait for the mutex to be free again. If
     * the mutex is exclusively acquired by a different thread this will return false.
     *
     * An @ref unlock_shared() call must be made for each call to this method that returns true.
     *
     * @see lock_shared(), unlock_shared()
     */
    bool try_lock_shared();
    /**
     * @brief      Releases the sjared ownership on the mutex
     *
     * This needs to be called for every @ref lock_shared() and for every @ref try_lock_shared()
     * that returns true. It should not be called without a matching @ref lock_shared() or
     * @ref try_lock_shared().
     *
     * @see lock_shared(), try_lock_shared()
     */
    void unlock_shared();

private:
    //! The state of the shared spin mutex.
    //! The first 2 LSB will indicate whether we have a writer or we are having a pending writer.
    //! The rest of the bits indicates the count of the readers.
    std::atomic<uintptr_t> lock_state_{0};

    //! Bitmask to check if we have a writer acquiring the mutex
    static constexpr uintptr_t has_writer_ = 1;
    //! Bitmask to check if somebody tries to acquire the mutex as writer
    static constexpr uintptr_t has_writer_pending_ = 2;
    //! Bitmask indicating that we have a writer, or a pending writer
    static constexpr uintptr_t has_writer_or_pending_ = has_writer_ | has_writer_pending_;
    //! Bitmask with that capture all the readers that we have
    static constexpr uintptr_t readers_ = ~(has_writer_or_pending_);
    //! Bitmask indicating whether we have a writer or readers
    static constexpr uintptr_t is_busy_ = (has_writer_ | readers_);
    //! The increment that we need to use for each reader
    static constexpr uintptr_t reader_increment_ = 4;
};

inline void shared_spin_mutex::lock() {
    spin_backoff spinner;
    while (true) {
        uintptr_t state = lock_state_.load();
        if (!(state & is_busy_)) {
            if (lock_state_.compare_exchange_weak(state, has_writer_))
                break;
            // Reset the spinner; we should be really-really close to take the lock
            spinner = spin_backoff{};
        } else if (!(state & has_writer_pending_)) {
            // Mark ourselves as trying to acquire to lock as writer
            lock_state_ |= has_writer_pending_;
        }
        spinner.pause();
    }
}
inline bool shared_spin_mutex::try_lock() {
    // If we don't have readers and writers, try once to make this a writer
    uintptr_t state = lock_state_.load();
    if (!(state & is_busy_)) {
        if (lock_state_.compare_exchange_strong(state, has_writer_))
            return true;
    }
    return false;
}
inline void shared_spin_mutex::unlock() {
    // Make sure to clear the has_writer_ and has_writer_pending_ flags
    lock_state_ &= readers_;
}

inline void shared_spin_mutex::lock_shared() {
    spin_backoff spinner;
    while (true) {
        uintptr_t state = lock_state_.load();
        // If we don't have any writer, or write requests...
        if (!(state & has_writer_or_pending_)) {
            // Try to add a reader, but make sure no writer is added at the same time
            if (lock_state_.compare_exchange_strong(state, state + reader_increment_))
                break;
        }
        spinner.pause();
    }
}
inline bool shared_spin_mutex::try_lock_shared() {
    // If we don't have a writer, try once to take reader lock
    uintptr_t state = lock_state_.load();
    if (!(state & has_writer_)) {
        if (lock_state_.compare_exchange_strong(state, state + reader_increment_))
            return true;
    }
    return false;
}
inline void shared_spin_mutex::unlock_shared() { lock_state_ -= reader_increment_; }

} // namespace v1
} // namespace concore