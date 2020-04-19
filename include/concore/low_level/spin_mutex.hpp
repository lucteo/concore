#pragma once

#include "spin_backoff.hpp"

#include <atomic>

namespace concore {
inline namespace v1 {

/**
 * @brief      Mutex class that uses CPU spinning while attempting to take the lock.
 *
 * For mutexes that protect very small regions of code, a spin_mutex can be much faster than a
 * traditional mutex. Instead of taking a lock, this will spin on the CPU, trying to avoid yielding
 * the CPU quanta.
 *
 * This uses an exponential backoff spinner. If after some time doing small waits it cannot enter
 * the  critical section, it will yield the CPU quanta of the current thread.
 * 
 * Spin mutexes should only be used to protect very-small regions of code; a handful of CPU
 * instructions. For larger scopes, a traditional mutex may be faster; but then, think about using
 * @ref serializer to avoid mutexes completely.
 *
 * @see spin_backoff
 */
class spin_mutex {
public:
    /**
     * @brief      Default constructor.
     * 
     * Constructs a spin mutex that is not acquired by any thread.
     */
    spin_mutex() = default;

    //! Copy constructor is DISABLED
    spin_mutex(const spin_mutex&) = delete;
    //! Copy assignment is DISABLED
    spin_mutex& operator=(const spin_mutex&) = delete;

    /**
     * @brief      Acquires ownership of the mutex
     * 
     * Uses a @ref spin_backoff to spin while waiting for the ownership to be free. When exiting
     * this function the mutex will be owned by the current thread.
     * 
     * An @ref unlock() call must be made for each call to lock().
     * 
     * @see try_lock(), unlock()
     */
    void lock() {
        spin_backoff spinner;
        while (busy_.test_and_set(std::memory_order_acquire)) {
            spinner.pause();
        }
    }
    /**
     * @brief      Tries to lock the mutex; returns false if the mutex is not available
     *
     * @return     True if the mutex ownership was acquired; false if the mutex is busy
     * 
     * This is similar to @ref lock() but does not wait for the mutex to be free again. If the mutex
     * is acquired by a different thread, this will return false.
     *             
     * An @ref unlock() call must be made for each call to this method that returns true.
     * 
     * @see lock(), unlock()
     */
    bool try_lock() { return !busy_.test_and_set(std::memory_order_acquire); }

    /**
     * @brief      Releases the ownership on the mutex
     * 
     * This needs to be called for every @ref lock() and for every @ref try_lock() that returns
     * true. It should not be called without a matching @ref lock() or @ref try_lock().
     * 
     * @see lock(), try_lock()
     */
    void unlock() { busy_.clear(std::memory_order_release); }

private:
    //! True if the spin mutex is taken
    std::atomic_flag busy_ = ATOMIC_FLAG_INIT;
};

} // namespace v1
} // namespace concore