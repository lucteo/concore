#pragma once

#include "../profiling.hpp"
#include "../detail/platform.hpp"

namespace concore {

inline namespace v1 {

#if !DOXYGEN_BUILD

#if defined(CONCORE_PLATFORM_CUSTOM_SEMAPHORE)

// Externally supplied; nothing to do
// We assume the following macros are defined:
//  - CONCORE_SEMAPHORE_IMPL_MEMBERS
//  - CONCORE_SEMAPHORE_IMPL_CTOR(start_count)
//  - CONCORE_SEMAPHORE_IMPL_DTOR()
//  - CONCORE_SEMAPHORE_IMPL_WAIT()
//  - CONCORE_SEMAPHORE_IMPL_SIGNAL()
//  - CONCORE_BINARY_SEMAPHORE_IMPL_MEMBERS
//  - CONCORE_BINARY_SEMAPHORE_IMPL_CTOR()
//  - CONCORE_BINARY_SEMAPHORE_IMPL_DTOR()
//  - CONCORE_BINARY_SEMAPHORE_IMPL_WAIT()
//  - CONCORE_BINARY_SEMAPHORE_IMPL_SIGNAL()

#elif defined(CONCORE_PLATFORM_LINUX)

// Synonym for sem_t
#define CONCORE_SEMAPHORE_IMPL_MEMBERS std::aligned_storage<32>::type sem_;
#define CONCORE_BINARY_SEMAPHORE_IMPL_MEMBERS CONCORE_SEMAPHORE_IMPL_MEMBERS

#elif defined(CONCORE_PLATFORM_APPLE)

// Synonym for semaphore_t
#define CONCORE_SEMAPHORE_IMPL_MEMBERS int sem_;
#define CONCORE_BINARY_SEMAPHORE_IMPL_MEMBERS CONCORE_SEMAPHORE_IMPL_MEMBERS

#elif defined(CONCORE_PLATFORM_WINDOWS)

// Synonym for HANDLE
#define CONCORE_SEMAPHORE_IMPL_MEMBERS void* sem_handle_;
#define CONCORE_BINARY_SEMAPHORE_IMPL_MEMBERS CONCORE_SEMAPHORE_IMPL_MEMBERS

#else

// In the most generic way, implement this with a conditional variable
#define CONCORE_SEMAPHORE_IMPL_MEMBERS                                                             \
    std::condition_variable cond_var_;                                                             \
    std::mutex mutex_;                                                                             \
    int count_;
#define CONCORE_BINARY_SEMAPHORE_IMPL_MEMBERS CONCORE_SEMAPHORE_IMPL_MEMBERS

#endif

#endif

/**
 * @brief      The classic "semaphore" synchronization primitive.
 *
 * It atomically maintains an internal count. The count can always be increased by calling signal(),
 * which is always a non-blocking call. When calling wait(), the count is decremented; if the count
 * is still positive the call will be non-blocking; if the count goes below zero, the call to wait()
 * will block until some other thread calls signal().
 *
 * @see binary_semaphore
 */
class semaphore {
public:
    /**
     * @brief      Constructs a new semaphore instance
     *
     * @param      start_count  The value that the semaphore count should have at start
     */
    explicit semaphore(int start_count = 0);
    //! Destructor
    ~semaphore();

    //! Copy constructor is DISABLED
    semaphore(const semaphore&) = delete;
    //! Copy assignment is DISABLED
    void operator=(const semaphore&) = delete;

    /**
     * @brief      Decrement the internal count and wait on the count to be positive
     *
     * If the count of the semaphore is positive this will decrement the count and return
     * immediately. On the other hand, if the count is 0, it wait for it to become positive before
     * decrementing it and returning.
     *
     * @see signal()
     */
    void wait();
    /**
     * @brief      Increment the internal count
     *
     * If there are at least one thread that is blocked inside a @ref wait() call, this will wake up
     * a waiting thread.
     *
     * @see wait()
     */
    void signal();

private:
    CONCORE_SEMAPHORE_IMPL_MEMBERS
};

/**
 * @brief      A semaphore that has two states: SIGNALED and WAITING
 *
 * It's assumed that the user will not call signal() multiple times.
 *
 * It may be implemented exactly as a @ref semaphore, but on some platforms it can be implemented
 * more efficiently.
 *
 * @see semaphore
 */
class binary_semaphore {
public:
    // Constructor. Puts the semaphore in the SIGNALED state
    binary_semaphore();
    //! Destructor
    ~binary_semaphore();

    //! Copy constructor is DISABLED
    binary_semaphore(const binary_semaphore&) = delete;
    //! Copy assignment is DISABLED
    void operator=(const binary_semaphore&) = delete;

    /**
     * @brief      Wait for the semaphore to be signaled.
     *
     * This will put the binary semaphore in the WAITING state, and wait for a thread to signal it.
     * The call will block until a corresponding thread will signal it.
     *
     * @see signal(0)
     */
    void wait();

    /**
     * @brief      Signal the binary semaphore
     *
     * Puts the semaphore in the SIGNALED state. If there is a thread that waits on the semaphore
     * it will wake it.
     */
    void signal();

private:
    CONCORE_BINARY_SEMAPHORE_IMPL_MEMBERS
};

} // namespace v1
} // namespace concore