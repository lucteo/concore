#pragma once

#include "spin_backoff.hpp"

namespace concore {
inline namespace v1 {

/**
 * @brief      Mutex class that uses CPU spinning while attempting to take the lock.
 *
 * For mutexes that protect very small regions of code, a spin_mutex can be much faster than a
 * traditional mutex. Instead of taking a lock, this will spin on the CPU, trying to avoid yielding
 * the CPU quanta.
 *
 * This uses an exponential backoff spinner.
 *
 * @see spin_backoff
 */
class spin_mutex {
public:
    spin_mutex() = default;

    // Copy is disabled
    spin_mutex(const spin_mutex&) = delete;
    spin_mutex& operator=(const spin_mutex&) = delete;

    //! Acquires ownership of the mutex; spins if the mutex is not available
    void lock() {
        spin_backoff spinner;
        while (busy_.test_and_set(std::memory_order_acquire)) {
            spinner.pause();
        }
    }
    //! Tries to lock the mutex; returns false if the mutex is not available
    bool try_lock() { return !busy_.test_and_set(std::memory_order_acquire); }

    //! Releases the ownership on the mutex
    void unlock() { busy_.clear(std::memory_order_release); }

private:
    //! True if the spin mutex is taken
    std::atomic_flag busy_ = ATOMIC_FLAG_INIT;
};

} // namespace v1
} // namespace concore