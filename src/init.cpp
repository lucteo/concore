#include "concore/init.hpp"
#include "concore/low_level/spin_mutex.hpp"
#include "concore/detail/platform.hpp"
#include "concore/detail/library_data.hpp"
#include "concore/detail/task_system.hpp"

#include <mutex>

#define __IMPL__CONCORE_USE_CXX_ABI CONCORE_CPP_COMPILER(gcc) || CONCORE_CPP_COMPILER(clang)

namespace concore {
namespace detail {

#if __IMPL__CONCORE_USE_CXX_ABI
#if CONCORE_CPU_ARCH(arm)
using cxa_guard_type = uint32_t;
#else
using cxa_guard_type = uint64_t;
#endif

static cxa_guard_type g_initialized_guard{0};
static task_system* g_task_system{nullptr};

// Take the advantage of ABI compatibility
extern "C" int __cxa_guard_acquire(cxa_guard_type*);
extern "C" void __cxa_guard_release(cxa_guard_type*);
extern "C" void __cxa_guard_abort(cxa_guard_type*);

#else

static std::atomic<task_system*> g_task_system{nullptr};

#endif

//! Identifier used to determine if the library was already initialized at the oint in which @ref
//! init() is called.
//! One should never dereference this pointer, as it might be invalid.
static std::atomic<const init_data*> g_config_used = nullptr;

//! Called to shutdown the library
void do_shutdown() {
    if (!is_initialized())
        return;
#if __IMPL__CONCORE_USE_CXX_ABI
    delete detail::g_task_system;
    detail::g_task_system = nullptr;
    g_initialized_guard = 0;
#else
    delete detail::g_task_system.load();
    detail::g_task_system.store(nullptr, std::memory_order_release);
#endif
    detail::g_config_used.store(nullptr, std::memory_order_release);
}

//! Actually initialized the library; this is guarded by get_task_system()
void do_init(const init_data* config) {
    static init_data default_config;
    if (!config)
        config = &default_config;
    auto ts = new task_system(*config);
#if __IMPL__CONCORE_USE_CXX_ABI
    detail::g_task_system = ts;
#else
    detail::g_task_system.store(ts, std::memory_order_release);
#endif
    g_config_used.store(config, std::memory_order_release);
    atexit(&do_shutdown);
}

task_system& get_task_system(const init_data* config) {
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

inline namespace v1 {

void init(const init_data& config) {
    if (is_initialized())
        throw already_initialized();
    detail::get_task_system(&config);
}

bool is_initialized() { return detail::g_task_system != nullptr; }

void shutdown() { detail::do_shutdown(); }

} // namespace v1

} // namespace concore