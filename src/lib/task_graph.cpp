#include "concore/task_graph.hpp"
#include "concore/spawn.hpp"
#include "concore/profiling.hpp"
#include "concore/detail/utils.hpp"
#include "concore/detail/enqueue_next.hpp"

#include <atomic>
#include <cassert>

namespace concore {

namespace detail {

//! The data for a chained_task
struct chained_task_impl : public std::enable_shared_from_this<chained_task_impl> {
    std::atomic<int32_t> pred_count_{0};
    std::vector<chained_task> next_tasks_;
    any_executor executor_;

    chained_task_impl(any_executor executor)
        : executor_(executor) {
        if (!executor)
            executor_ = concore::spawn_executor{};
    }

    //! Called whenever this task is done, to continue with the execution of the graph
    void on_cont(std::exception_ptr) noexcept {
        CONCORE_PROFILING_SCOPE_N("chained_task.on_cont");
        // Try to execute the next tasks
        for (auto& n : next_tasks_) {
            if (n.impl_->pred_count_-- == 1) {
                chained_task next(std::move(n));      // don't keep the ref here anymore
                executor_.execute(*next.to_execute_); // execute a copy of the task
            }
        }
        next_tasks_.clear();
    }

    //! Set the continuation of the task, so that it executes the next tasks in the graph.
    //! If the task already has a continuation, that would be called first.
    void set_continuation(task& t) {
        auto inner_cont = t.get_continuation();
        task_continuation_function cont;
        if (inner_cont) {
            cont = [inner_cont, p_this = shared_from_this()](std::exception_ptr ex) {
                inner_cont(ex);
                p_this->on_cont(std::move(ex));
            };
        } else {
            cont = [p_this = shared_from_this()](
                           std::exception_ptr ex) { p_this->on_cont(std::move(ex)); };
        }
        t.set_continuation(std::move(cont));
    }
};

} // namespace detail

inline namespace v1 {

chained_task::chained_task(task t, any_executor executor)
    : impl_(std::make_shared<detail::chained_task_impl>(std::move(executor)))
    , to_execute_(std::make_shared<task>(std::move(t))) {
    impl_->set_continuation(*to_execute_);
}

void chained_task::operator()() noexcept {
    CONCORE_PROFILING_SCOPE_N("chained_task.()");
    assert(impl_->pred_count_.load() == 0);

    // Execute the current task
    (*to_execute_)();
}

chained_task::operator bool() const noexcept { return static_cast<bool>(impl_); }

void chained_task::clear_next() noexcept {
    for (auto& n : impl_->next_tasks_)
        n.impl_->pred_count_--;
    impl_->next_tasks_.clear();
}

void add_dependency(chained_task prev, chained_task next) {
    next.impl_->pred_count_++;
    prev.impl_->next_tasks_.push_back(next);
}
void add_dependencies(chained_task prev, std::initializer_list<chained_task> nexts) {
    for (const auto& n : nexts)
        n.impl_->pred_count_++;
    prev.impl_->next_tasks_.insert(prev.impl_->next_tasks_.end(), nexts.begin(), nexts.end());
}
void add_dependencies(std::initializer_list<chained_task> prevs, chained_task next) {
    next.impl_->pred_count_ += static_cast<int32_t>(prevs.size());
    for (const auto& p : prevs)
        p.impl_->next_tasks_.push_back(next);
}

} // namespace v1
} // namespace concore
