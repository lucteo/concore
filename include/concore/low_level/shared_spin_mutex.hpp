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
 * This can be locked in two ways:
 *  - exclusive access (lock()/try_lock()/unlock()) -- similar to a regular mutex
 *  - shared access (lock_shared()/try_lock_shared()/unlock_shared()) -- multiple threads can have
 *    shared ownership of the lock, but no exclusive access is permitted during this time.
 *
 * This uses an exponential backoff spinner.
 *
 * @see spin_mutex, spin_backoff
 */
class shared_spin_mutex {
public:
    shared_spin_mutex() = default;

    // Copy is disabled
    shared_spin_mutex(const shared_spin_mutex&) = delete;
    shared_spin_mutex& operator=(const shared_spin_mutex&) = delete;

    //! Acquires exclusive ownership of the mutex; spins if the mutex is not available
    void lock();
    //! Tries to lock the mutex exclusively; returns false if the mutex is not available
    bool try_lock();
    //! Releases the exclusive ownership on the mutex
    void unlock();

    //! Acquires shared ownership of the mutex; spins if the mutex is not available.
    void lock_shared();
    //! Tries to acquire shared ownership of the mutex; returns false on failure
    bool try_lock_shared();
    //! Releases the shared ownership of the mutex
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