#pragma once

#include "detail/platform.hpp"

#if CONCORE_USE_LIBDISPATCH

#include <dispatch/dispatch.h>

#include "profiling.hpp"

namespace concore {
inline namespace v1 {
namespace detail_disp {

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

} // namespace detail_disp

//! The default libdispatch task executor. This will enqueue a task with a "normal" priority in the
//! system, and whenever we have a core available to execute it, it will be executed.
constexpr auto dispatch_executor =
        detail_disp::executor_with_prio<detail_disp::task_priority::normal>{};

//! Task executor that enqueues tasks with "high" priority.
constexpr auto dispatch_executor_high_prio =
        detail_disp::executor_with_prio<detail_disp::task_priority::high>{};
//! Task executor that enqueues tasks with "normal" priority. Same as `dispatch_executor`.
constexpr auto dispatch_executor_normal_prio =
        detail_disp::executor_with_prio<detail_disp::task_priority::normal>{};
//! Task executor that enqueues tasks with "low" priority.
constexpr auto dispatch_executor_low_prio =
        detail_disp::executor_with_prio<detail_disp::task_priority::low>{};

} // namespace v1
} // namespace concore

#endif
