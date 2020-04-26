#pragma once

#if CONCORE_USE_TBB || DOXYGEN_BUILD

#include <tbb/task.h>

#include "profiling.hpp"

namespace concore {
namespace detail {
namespace tbb_d {

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

} // namespace tbb_d
} // namespace detail

inline namespace v1 {
/**
 * @brief      Executor that enqueues task in TBB.
 *
 * The tasks that are enqueued by this executor will have *normal* priority inside TBB.
 *
 * This can be used as a bridge between concore and Intel TBB.
 *
 * @see        global_executor
 */
constexpr auto tbb_executor =
        detail::tbb_d::executor_with_prio<detail::tbb_d::task_priority::normal>{};

/**
 * @brief      Task executor that enqueues tasks in TBB with *high* priority.
 *
 * This can be used as a bridge between concore and Intel TBB.
 */
constexpr auto tbb_executor_high_prio =
        detail::tbb_d::executor_with_prio<detail::tbb_d::task_priority::high>{};
/**
 * @brief      Task executor that enqueues tasks in TBB with *normal* priority.
 *
 * Same as @ref tbb_executor.
 *
 * This can be used as a bridge between concore and Intel TBB.
 */
constexpr auto tbb_executor_normal_prio =
        detail::tbb_d::executor_with_prio<detail::tbb_d::task_priority::normal>{};

/**
 * @brief      Task executor that enqueues tasks in TBB with *low* priority.
 *
 * This can be used as a bridge between concore and Intel TBB.
 */
constexpr auto tbb_executor_low_prio =
        detail::tbb_d::executor_with_prio<detail::tbb_d::task_priority::low>{};

} // namespace v1
} // namespace concore

#endif