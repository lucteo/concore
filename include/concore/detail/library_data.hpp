#pragma once

#include "platform.hpp"
#include <concore/low_level/spin_mutex.hpp>

#include <mutex>

#define __IMPL__CONCORE_USE_CXX_ABI CONCORE_CPP_COMPILER(gcc) || CONCORE_CPP_COMPILER(clang)

#if __IMPL__CONCORE_USE_CXX_ABI
#include <cxxabi.h>
#endif

namespace concore {

inline namespace v1 { struct init_data; }

namespace detail {

class task_system;

#if __IMPL__CONCORE_USE_CXX_ABI
#if CONCORE_CPU_ARCH(arm)
using cxa_guard_type = uint32_t;
#else
using cxa_guard_type = uint64_t;
#endif

extern cxa_guard_type g_initialized_guard;
extern task_system* g_task_system;

extern "C" int __cxa_guard_acquire(cxa_guard_type*);
extern "C" void __cxa_guard_release(cxa_guard_type*);
extern "C" void __cxa_guard_abort(cxa_guard_type*);


#else
extern std::atomic<task_system*> g_task_system;
#endif

/**
 * @brief      The function that actually initializes concore.
 *
 * @param      config  The configuration to be used for the library; can be null.
 *
 * This will be called once to initialize the library, especially the task_system object.
 */
void do_init(const init_data* config);

/**
 * @brief      Performs the shutdown of the library.
 */
void do_shutdown();

/**
 * @brief      Getter for the task_system object that also ensures that the library is initialized.
 *
 * @param      config  The configuration to be used for the library; can be null.
 * 
 * @return     The task system object to be used inside concore.
 *
 * This is used so that we can automatically initialize the library before its first use.
 */
inline task_system& get_task_system(const init_data* config = nullptr) {
#if __IMPL__CONCORE_USE_CXX_ABI
    if (__cxa_guard_acquire(&g_initialized_guard)) {
        try {
            do_init(config);

        } catch (...) {
            __cxa_guard_abort(&g_initialized_guard);
            throw;
        }
        __cxa_guard_release(&g_initialized_guard);
    }
    return *g_task_system;
#else
    auto p = detail::g_task_system.load(std::memory_order_acquire);
    if (!p) {
        static spin_mutex init_bottleneck;
        try {
            init_bottleneck.lock();
            do_init(config);
            init_bottleneck.unlock();
            p = g_task_system.load(std::memory_order_acquire);
        } catch (...) {
            init_bottleneck.unlock();
            throw;
        }
    }
    return *p;
#endif
}

} // namespace detail
} // namespace concore