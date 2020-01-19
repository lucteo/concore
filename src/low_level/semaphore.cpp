#include "concore/low_level/semaphore.hpp"
#include "concore/profiling.hpp"

#include <cassert>

#if defined(CONCORE_PLATFORM_CUSTOM_SEMAPHORE)

// Externally supplied; nothing to do

#elif defined(CONCORE_PLATFORM_LINUX)

#include <semaphore.h>

#define CONCORE_SEMAPHORE_IMPL_CTOR(start_count)                                                   \
    static_assert(sizeof(sem_t) <= sizeof(sem_), "Did not find the right size of sem_t");          \
    int ret = sem_init(reinterpret_cast<sem_t*>(&sem_), 0, start_count);                           \
    assert(!ret);
#define CONCORE_SEMAPHORE_IMPL_DTOR()                                                              \
    int ret = sem_destroy(reinterpret_cast<sem_t*>(&sem_));                                        \
    assert(!ret);

#define CONCORE_SEMAPHORE_IMPL_WAIT()                                                              \
    while (sem_wait(reinterpret_cast<sem_t*>(&sem_)) != 0)                                         \
        ;
#define CONCORE_SEMAPHORE_IMPL_SIGNAL() sem_post(reinterpret_cast<sem_t*>(&sem_))

#define CONCORE_BINARY_SEMAPHORE_IMPL_CTOR() CONCORE_SEMAPHORE_IMPL_CTOR(0)
#define CONCORE_BINARY_SEMAPHORE_IMPL_DTOR CONCORE_SEMAPHORE_IMPL_DTOR
#define CONCORE_BINARY_SEMAPHORE_IMPL_WAIT CONCORE_SEMAPHORE_IMPL_WAIT
#define CONCORE_BINARY_SEMAPHORE_IMPL_SIGNAL CONCORE_SEMAPHORE_IMPL_SIGNAL

#elif defined(CONCORE_PLATFORM_APPLE)

#include <mach/semaphore.h>
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach/error.h>

#define CONCORE_SEMAPHORE_IMPL_CTOR(start_count)                                                   \
    static_assert(                                                                                 \
            sizeof(semaphore_t) == sizeof(sem_), "Did not find the right size of semaphore_t");    \
    auto ret = semaphore_create(mach_task_self(), reinterpret_cast<semaphore_t*>(&sem_),           \
            SYNC_POLICY_FIFO, start_count);                                                        \
    assert(ret == err_none);
#define CONCORE_SEMAPHORE_IMPL_DTOR()                                                              \
    auto ret = semaphore_destroy(mach_task_self(), *reinterpret_cast<semaphore_t*>(&sem_));        \
    assert(ret == err_none);

#define CONCORE_SEMAPHORE_IMPL_WAIT()                                                              \
    int ret;                                                                                       \
    do {                                                                                           \
        ret = semaphore_wait(*reinterpret_cast<semaphore_t*>(&sem_));                              \
    } while (ret == KERN_ABORTED);                                                                 \
    assert(ret == KERN_SUCCESS);
#define CONCORE_SEMAPHORE_IMPL_SIGNAL() semaphore_signal(*reinterpret_cast<semaphore_t*>(&sem_))

#define CONCORE_BINARY_SEMAPHORE_IMPL_CTOR() CONCORE_SEMAPHORE_IMPL_CTOR(0)
#define CONCORE_BINARY_SEMAPHORE_IMPL_DTOR CONCORE_SEMAPHORE_IMPL_DTOR
#define CONCORE_BINARY_SEMAPHORE_IMPL_WAIT CONCORE_SEMAPHORE_IMPL_WAIT
#define CONCORE_BINARY_SEMAPHORE_IMPL_SIGNAL CONCORE_SEMAPHORE_IMPL_SIGNAL

#elif defined(CONCORE_PLATFORM_WINDOWS)

#include <windows.h>

#define CONCORE_SEMAPHORE_IMPL_CTOR(start_count)                                                   \
    static_assert(sizeof(HANDLE) == sizeof(sem_handle_), "Did not find the right size of HANDLE"); \
    sem_handle_ = CreateSemaphoreEx(NULL, LONG(start_count), MAXLONG, NULL, 0, SEMAPHORE_ALL_ACCESS)
#define CONCORE_SEMAPHORE_IMPL_DTOR() CloseHandle(sem_handle_)

#define CONCORE_SEMAPHORE_IMPL_WAIT() WaitForSingleObjectEx(sem_handle_, INFINITE, FALSE)
#define CONCORE_SEMAPHORE_IMPL_SIGNAL() ReleaseSemaphore(sem_handle_, 1, NULL)

// Use events for binary semaphores
#define CONCORE_BINARY_SEMAPHORE_IMPL_CTOR()                                                       \
    static_assert(sizeof(HANDLE) == sizeof(sem_handle_), "Did not find the right size of HANDLE"); \
    sem_handle_ = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS)

#define CONCORE_BINARY_SEMAPHORE_IMPL_DTOR() CloseHandle(sem_handle_)
#define CONCORE_BINARY_SEMAPHORE_IMPL_WAIT() WaitForSingleObjectEx(sem_handle_, INFINITE, FALSE)
#define CONCORE_BINARY_SEMAPHORE_IMPL_SIGNAL() SetEvent(sem_handle_)

#else

// In the most generic way, implement this with a conditional variable
#define CONCORE_SEMAPHORE_IMPL_CTOR(start_count) count_ = start_count
#define CONCORE_SEMAPHORE_IMPL_DTOR() /*nothing*/

#define CONCORE_SEMAPHORE_IMPL_WAIT()                                                              \
    std::unique_lock<std::mutex> lock{mutex_};                                                     \
    while (count_ <= 0) {                                                                          \
        cond_var_.wait(lock);                                                                      \
    }                                                                                              \
    count_--;
#define CONCORE_SEMAPHORE_IMPL_SIGNAL()                                                            \
    bool should_signal = false;                                                                    \
    {                                                                                              \
        std::unique_lock<std::mutex> lock{mutex_};                                                 \
        should_signal = count_++ >= -1;                                                            \
    }                                                                                              \
    if (should_signal)                                                                             \
        cond_var_.notify_all();

#define CONCORE_BINARY_SEMAPHORE_IMPL_CTOR() CONCORE_SEMAPHORE_IMPL_CTOR(0)
#define CONCORE_BINARY_SEMAPHORE_IMPL_DTOR CONCORE_SEMAPHORE_IMPL_DTOR
#define CONCORE_BINARY_SEMAPHORE_IMPL_WAIT CONCORE_SEMAPHORE_IMPL_WAIT
#define CONCORE_BINARY_SEMAPHORE_IMPL_SIGNAL CONCORE_SEMAPHORE_IMPL_SIGNAL

#endif

namespace concore {
inline namespace v1 {

semaphore::semaphore(int start_count) {
    CONCORE_PROFILING_FUNCTION();
    CONCORE_SEMAPHORE_IMPL_CTOR(start_count);
}

semaphore::~semaphore() {
    CONCORE_PROFILING_FUNCTION();
    CONCORE_SEMAPHORE_IMPL_DTOR();
}

void semaphore::wait() {
    CONCORE_PROFILING_SCOPE_C(CONCORE_PROFILING_COLOR_SILVER);
    char buf[64];
    sprintf(buf, "this=%p", this);
    CONCORE_PROFILING_SET_TEXT(buf);
    CONCORE_SEMAPHORE_IMPL_WAIT();
}

void semaphore::signal() {
    CONCORE_PROFILING_FUNCTION();
    char buf[64];
    sprintf(buf, "this=%p", this);
    CONCORE_PROFILING_SET_TEXT(buf);
    CONCORE_SEMAPHORE_IMPL_SIGNAL();
}

binary_semaphore::binary_semaphore() {
    CONCORE_PROFILING_FUNCTION();
    CONCORE_BINARY_SEMAPHORE_IMPL_CTOR();
}

binary_semaphore::~binary_semaphore() {
    CONCORE_PROFILING_FUNCTION();
    CONCORE_BINARY_SEMAPHORE_IMPL_DTOR();
}

void binary_semaphore::wait() {
    CONCORE_PROFILING_FUNCTION();
    CONCORE_BINARY_SEMAPHORE_IMPL_WAIT();
    char buf[64];
    sprintf(buf, "this=%p", this);
    CONCORE_PROFILING_SET_TEXT(buf);
}

void binary_semaphore::signal() {
    CONCORE_PROFILING_FUNCTION();
    CONCORE_BINARY_SEMAPHORE_IMPL_SIGNAL();
    char buf[64];
    sprintf(buf, "this=%p", this);
    CONCORE_PROFILING_SET_TEXT(buf);
}

} // namespace v1
} // namespace concore