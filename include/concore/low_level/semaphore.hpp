#pragma once

#include "../profiling.hpp"
#include "../detail/platform.hpp"

namespace concore {

inline namespace v1 {

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
    explicit semaphore(int start_count = 0);
    ~semaphore();

    // No copy
    semaphore(const semaphore&) = delete;
    void operator=(const semaphore&) = delete;

    //! Decrement the internal count and wait on the count to be positive
    void wait();
    //! Increment the internal count
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
    binary_semaphore();
    ~binary_semaphore();

    // No copy
    binary_semaphore(const binary_semaphore&) = delete;
    void operator=(const binary_semaphore&) = delete;

    //! Puts the semaphore in the WAITING state and wait for a call to @ref signal()
    void wait();

    //! Puts the semaphore in the SIGNALED state
    void signal();

private:
    CONCORE_BINARY_SEMAPHORE_IMPL_MEMBERS
};

} // namespace v1
} // namespace concore