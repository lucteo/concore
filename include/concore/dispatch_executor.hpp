#pragma once

#include "detail/platform.hpp"

#if CONCORE_USE_LIBDISPATCH || DOXYGEN_BUILD

#include <dispatch/dispatch.h>

#include "profiling.hpp"

namespace concore {
namespace detail {
namespace disp {

//! The possible priorities of tasks, as handled by the dispatch executor
enum class task_priority {
    high = DISPATCH_QUEUE_PRIORITY_HIGH,      //! High-priority tasks
    normal = DISPATCH_QUEUE_PRIORITY_DEFAULT, //! Tasks with normal priority
    low = DISPATCH_QUEUE_PRIORITY_LOW,        //! Tasks with low priority
};

//! Wrapper over a libdispatch group object
struct group_wrapper {
    dispatch_group_t group_{dispatch_group_create()};

    ~group_wrapper() {
        dispatch_group_wait(group_, DISPATCH_TIME_FOREVER);
        dispatch_release(group_);
    }
};

//! Structure that defines an executor for a given priority that uses libdispatch.
//!
//! Note that this will allocate memory for each task, to keep the task alive.
//! As the tasks are dynamic, we prefer to get them as functors instead of the fixed `task` type.
template <task_priority P = task_priority::normal>
struct executor_with_prio {
    template <typename T>
    void operator()(T t) const {
        CONCORE_PROFILING_SCOPE_N("enqueue");

        using task_type = decltype(t);
        static group_wrapper g;
        auto queue = dispatch_get_global_queue(static_cast<long>(P), 0);
        auto context = new task_type(std::move(t));
        auto work = [](void* ctx) {
            CONCORE_PROFILING_SCOPE_N("execute_task");
            auto f = static_cast<task_type*>(ctx);
            try {
                (*f)();
            } catch (...) {
            }
            // Delete the task after executing it
            delete f;
        };
        dispatch_group_async_f(g.group_, queue, context, work);
    }
};

} // namespace disp
} // namespace detail

inline namespace v1 {
/**
 * @brief      Executor that enqueues task in libdispatch.
 * 
 * The tasks that are enqueued by this executor will have *normal* priority inside libdispatch.
 * 
 * This can be used as a bridge between concore and libdispatch.
 * 
 * @see        global_executor
 */
constexpr auto dispatch_executor =
        detail::disp::executor_with_prio<detail::disp::task_priority::normal>{};

/**
 * @brief      Task executor that enqueues tasks in libdispatch with *high* priority.
 * 
 * This can be used as a bridge between concore and libdispatch.
 */
constexpr auto dispatch_executor_high_prio =
        detail::disp::executor_with_prio<detail::disp::task_priority::high>{};
/**
 * @brief      Task executor that enqueues tasks in libdispatch with *normal* priority.
 * 
 * Same as @ref dispatch_executor.
 * 
 * This can be used as a bridge between concore and libdispatch.
 */
constexpr auto dispatch_executor_normal_prio =
        detail::disp::executor_with_prio<detail::disp::task_priority::normal>{};

/**
 * @brief      Task executor that enqueues tasks in libdispatch with *low* priority.
 * 
 * This can be used as a bridge between concore and libdispatch.
 */
constexpr auto dispatch_executor_low_prio =
        detail::disp::executor_with_prio<detail::disp::task_priority::low>{};

} // namespace v1
} // namespace concore

#endif
