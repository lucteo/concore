#pragma once

#include "detail/platform.hpp"
#include "detail/extra_type_traits.hpp"

#if CONCORE_USE_LIBDISPATCH || DOXYGEN_BUILD

#include <dispatch/dispatch.h>

#include "profiling.hpp"

namespace concore {
namespace detail {
//! Wrapper over a libdispatch group object
struct group_wrapper {
    dispatch_group_t group_{dispatch_group_create()};

    ~group_wrapper() {
        dispatch_group_wait(group_, DISPATCH_TIME_FOREVER);
        dispatch_release(group_);
    }
};
} // namespace detail

inline namespace v1 {

/**
 * @brief Executor that sends tasks to libdispatch
 *
 * This executors wraps the task execution from libdispatch.
 *
 * This executor provides just basic support for executing tasks, but not other features like
 * cancellation, waiting for tasks, etc.
 *
 * The executor takes as constructor parameter the priority of the task to be used when enqueueing
 * the task.
 *
 * Two executor objects are equivalent if their priorities match.
 *
 * @see global_executor
 */
struct dispatch_executor {

    //! The priority of the task to be used
    enum priority {
        prio_high = DISPATCH_QUEUE_PRIORITY_HIGH,      //! High-priority tasks
        prio_normal = DISPATCH_QUEUE_PRIORITY_DEFAULT, //! Tasks with normal priority
        prio_low = DISPATCH_QUEUE_PRIORITY_LOW,        //! Tasks with low priority
    };

    explicit dispatch_executor(priority prio = prio_normal)
        : prio_(prio) {}

    template <typename F>
    void execute(F&& f) const {
        CONCORE_PROFILING_SCOPE_N("enqueue");

        using task_type = detail::remove_cvref_t<decltype(f)>;
        static detail::group_wrapper g;
        auto queue = dispatch_get_global_queue(static_cast<long>(prio_), 0);
        auto context = new task_type(std::forward<F>(f));
        auto work = [](void* ctx) {
            CONCORE_PROFILING_SCOPE_N("libdispatch execute");
            auto ff = static_cast<task_type*>(ctx);
            try {
                (*ff)();
            } catch (...) {
            }
            // Delete the task after executing it
            delete ff;
        };
        dispatch_group_async_f(g.group_, queue, context, work);
    }

    friend inline bool operator==(dispatch_executor l, dispatch_executor r) {
        return l.prio_ == r.prio_;
    }
    friend inline bool operator!=(dispatch_executor l, dispatch_executor r) { return !(l == r); }

private:
    priority prio_;
};

} // namespace v1
} // namespace concore

#endif
