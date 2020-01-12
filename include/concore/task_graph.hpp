#pragma once

#include "task.hpp"
#include "executor_type.hpp"
#include "profiling.hpp"

#include <memory>
#include <atomic>
#include <vector>
#include <initializer_list>

namespace concore {

inline namespace v1 { class chained_task; }

namespace detail {

//! The data for a chained_task
struct chained_task_impl : public std::enable_shared_from_this<chained_task_impl> {
    task task_fun_;
    std::atomic<int32_t> pred_count_{0};
    std::vector<chained_task> next_tasks_;
    executor_t executor_;

    chained_task_impl(task t, executor_t executor)
        : task_fun_(std::move(t))
        , executor_(executor) {}
};

} // namespace detail

inline namespace v1 {

class chained_task;

//! A task that can be chained with other tasks to create a graph of tasks.
//!
//! One can create multiple `chained_task` objects, then call add_dependency() or add_dependencies()
//! to create dependencies between the task objects.
//!
//! The user is supposed to enqueue a task that has no predecessors. If the graph is properly
//! created (i.e., without cycles), then this will trigger the execution of other tasks in the
//! graph. Eventually, all the chained tasks will be executed.
//!
//! A chained tasks will be executed only after all the predecessors have been executed. The
//! execution trigger will be the last predecessor that will finish executing.
//!
//! Any exception thrown by the task will be ignored, and the next tasks will still be executed.
class chained_task {
public:
    chained_task(task t, executor_t executor)
        : impl_(std::make_shared<detail::chained_task_impl>(std::move(t), executor)) {}

    //! The call operator, corresponding to a task
    void operator()() {
        CONCORE_PROFILING_SCOPE_N("chained_task.()");
        assert(impl_->pred_count_.load() == 0);
        // Execute the current task
        try {
            impl_->task_fun_();
        } catch (...) {
        }

        // Try to execute the next tasks
        for (auto& n : impl_->next_tasks_) {
            if (n.impl_->pred_count_-- == 1) {
                chained_task next(std::move(n)); // don't keep the ref here anymore
                impl_->executor_(next);
            }
        }
        impl_->next_tasks_.clear();
    }

private:
    std::shared_ptr<detail::chained_task_impl> impl_;

    friend void add_dependency(chained_task, chained_task);
    friend void add_dependencies(chained_task, std::initializer_list<chained_task>);
    friend void add_dependencies(std::initializer_list<chained_task>, chained_task);
};

//! Add a dependency between two tasks.
inline void add_dependency(chained_task prev, chained_task next) {
    next.impl_->pred_count_++;
    prev.impl_->next_tasks_.push_back(next);
}

//! Add a dependency from a task to a list of tasks
inline void add_dependencies(chained_task prev, std::initializer_list<chained_task> nexts) {
    for (auto n : nexts)
        n.impl_->pred_count_++;
    prev.impl_->next_tasks_.insert(prev.impl_->next_tasks_.end(), nexts.begin(), nexts.end());
}
//! Add a dependency from list of tasks to a tasks
inline void add_dependencies(std::initializer_list<chained_task> prevs, chained_task next) {
    next.impl_->pred_count_ += static_cast<int32_t>(prevs.size());
    for (auto p : prevs)
        p.impl_->next_tasks_.push_back(next);
}

} // namespace v1
} // namespace concore