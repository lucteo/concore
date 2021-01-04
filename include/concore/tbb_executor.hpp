#pragma once

#if CONCORE_USE_TBB || DOXYGEN_BUILD

#include <tbb/task.h>

#include "profiling.hpp"

namespace concore {
inline namespace v1 {

/**
 * @brief Executor that sends tasks to TBB
 *
 * This executors wraps the task execution from TBB.
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
struct tbb_executor {

    //! The priority of the task to be used
    enum priority {
        prio_high = tbb::priority_high,     //! High-priority tasks
        prio_normal = tbb::priority_normal, //! Tasks with normal priority
        prio_low = tbb::priority_low,       //! Tasks with low priority
    };

    explicit tbb_executor(priority prio = prio_normal)
        : prio_(prio) {}

    template <typename F>
    void execute(F&& f) const {
        CONCORE_PROFILING_SCOPE_N("enqueue");

        struct task_wrapper : tbb::task {
            F functor_;

            task_wrapper(F&& functor)
                : functor_(std::forward<F>(functor)) {}

            tbb::task* execute() override {
                CONCORE_PROFILING_SCOPE_N("TBB execute");
                std::forward<F>(functor_)();
                return nullptr;
            }
        };
        // start the task
        auto& tbb_task = *new (tbb::task::allocate_root()) task_wrapper(std::forward<F>(f));
        tbb::task::enqueue(tbb_task, static_cast<tbb::priority_t>(prio_));
    }

    friend inline bool operator==(tbb_executor l, tbb_executor r) { return l.prio_ == r.prio_; }
    friend inline bool operator!=(tbb_executor l, tbb_executor r) { return !(l == r); }

private:
    priority prio_;
};

} // namespace v1
} // namespace concore

#endif