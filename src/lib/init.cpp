#include "concore/init.hpp"
#include "concore/low_level/spin_mutex.hpp"
#include "concore/detail/platform.hpp"
#include "concore/detail/likely.hpp"
#include "concore/detail/library_data.hpp"
#include "concore/detail/exec_context.hpp"

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
static exec_context* g_exec_context{nullptr};

// Take the advantage of ABI compatibility
extern "C" int __cxa_guard_acquire(cxa_guard_type*);
extern "C" void __cxa_guard_release(cxa_guard_type*);
extern "C" void __cxa_guard_abort(cxa_guard_type*);

#else

static std::atomic<exec_context*> g_exec_context{nullptr};

#endif

//! Copy of the init_data used to create the global exec_context
init_data g_init_data_used;

//! The per-thread execution context; can be null if the thread doesn't belong to any execution
//! context.
thread_local exec_context* g_tlsCtx{nullptr};

//! Called to shutdown the library
void do_shutdown() {
    if (!is_initialized())
        return;
#if __IMPL__CONCORE_USE_CXX_ABI
    delete detail::g_exec_context;
    detail::g_exec_context = nullptr;
    g_initialized_guard = 0;
#else
    delete detail::g_exec_context.load();
    detail::g_exec_context.store(nullptr, std::memory_order_release);
#endif
}

//! Actually initialized the library; this is guarded by get_exec_context()
void do_init(const init_data* config) {
    static init_data default_config;
    if (!config)
        config = &default_config;
    auto global_ctx = new exec_context(*config);
    g_init_data_used = *config;
#if __IMPL__CONCORE_USE_CXX_ABI
    detail::g_exec_context = global_ctx;
#else
    detail::g_exec_context.store(global_ctx, std::memory_order_release);
#endif
    atexit(&do_shutdown);
}

exec_context& get_exec_context(const init_data* config) {
    // If we have an execution context in the current thread, return it
    if (g_tlsCtx)
        return *g_tlsCtx;

#if __IMPL__CONCORE_USE_CXX_ABI
    CONCORE_IF_UNLIKELY(__cxa_guard_acquire(&g_initialized_guard)) {
        try {
            do_init(config);

        } catch (...) {
            __cxa_guard_abort(&g_initialized_guard);
            throw;
        }
        __cxa_guard_release(&g_initialized_guard);
    }
    return *g_exec_context;
#else
    auto p = detail::g_exec_context.load(std::memory_order_acquire);
    CONCORE_IF_UNLIKELY(!p) {
        static spin_mutex init_bottleneck;
        try {
            init_bottleneck.lock();
            do_init(config);
            init_bottleneck.unlock();
            p = g_exec_context.load(std::memory_order_acquire);
        } catch (...) {
            init_bottleneck.unlock();
            throw;
        }
    }
    return *p;
#endif
}

void set_context_in_current_thread(exec_context* ctx) { g_tlsCtx = ctx; }

init_data get_current_init_data() { return g_init_data_used; }

} // namespace detail

inline namespace v1 {

void init(const init_data& config) {
    if (is_initialized())
        throw already_initialized();
    detail::get_exec_context(&config);
}

bool is_initialized() { return detail::g_exec_context != nullptr; }

void shutdown() { detail::do_shutdown(); }

} // namespace v1

} // namespace concore