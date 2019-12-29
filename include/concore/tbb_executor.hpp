#pragma once

#if CONCORE_USE_TBB

#include <tbb/task.h>

#include "profiling.hpp"

namespace concore {
inline namespace v1 {
namespace detail_tbb {

//! The possible priorities of tasks, as handled by the dispatch executor
enum class task_priority {
    high = tbb::priority_high,     //! High-priority tasks
    normal = tbb::priority_normal, //! Tasks with normal priority
    low = tbb::priority_low,       //! Tasks with low priority
};

//! Structure that defines an executor that uses Intel TBB to enqueue tasks
//!
//! This will use the raw task interface of TBB.
template <task_priority P = task_priority::normal>
struct executor_with_prio {
    template <typename T>
    void operator()(T t) const {
        CONCORE_PROFILING_SCOPE_N("enqueue");

        struct task_wrapper : tbb::task {
            T functor_;

            task_wrapper(T&& functor)
                : functor_(functor) {}

            tbb::task* execute() override {
                functor_();
                return nullptr;
            }
        };
        // start the task
        auto& tbb_task = *new (tbb::task::allocate_root()) task_wrapper(std::move(t));
        tbb::task::enqueue(tbb_task, static_cast<tbb::priority_t>(P));
    }
};

} // namespace detail_tbb

//! The default TBB task executor. This will enqueue a task with a "normal" priority in the
//! system, and whenever we have a core available to execute it, it will be executed.
constexpr auto tbb_executor = detail_tbb::executor_with_prio<detail_tbb::task_priority::normal>{};

//! Task executor that enqueues tasks with "high" priority.
constexpr auto tbb_executor_high_prio =
        detail_tbb::executor_with_prio<detail_tbb::task_priority::high>{};
//! Task executor that enqueues tasks with "normal" priority. Same as `tbb_executor`.
constexpr auto tbb_executor_normal_prio =
        detail_tbb::executor_with_prio<detail_tbb::task_priority::normal>{};
//! Task executor that enqueues tasks with "low" priority.
constexpr auto tbb_executor_low_prio =
        detail_tbb::executor_with_prio<detail_tbb::task_priority::low>{};

} // namespace v1
} // namespace concore

#endif