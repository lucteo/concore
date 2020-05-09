#include "concore/init.hpp"
#include "concore/detail/library_data.hpp"
#include "concore/detail/task_system.hpp"

namespace concore {
namespace detail {

#if __IMPL__CONCORE_USE_CXX_ABI
cxa_guard_type g_initialized_guard = 0;
task_system* g_task_system{nullptr};
#else
std::atomic<task_system*> g_task_system{nullptr};
#endif

//! Identifier used to determine if the library was already initialized at the oint in which @ref
//! init() is called.
//! One should never dereference this pointer, as it might be invalid.
static std::atomic<const init_data*> g_config_used = nullptr;

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

} // namespace detail

inline namespace v1 {

void init(const init_data& config) {
    if ( is_initialized() )
        throw already_initialized();
    detail::get_task_system(&config);
}

bool is_initialized() { return detail::g_task_system != nullptr; }

void shutdown() { detail::do_shutdown(); }

} // namespace v1

} // namespace concore