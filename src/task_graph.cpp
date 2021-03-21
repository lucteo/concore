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
    task task_fun_;
    std::atomic<int32_t> pred_count_{0};
    std::vector<chained_task> next_tasks_;
    any_executor executor_;
    except_fun_t except_fun_;

    chained_task_impl(task t, any_executor executor)
        : task_fun_(std::move(t))
        , executor_(executor) {
        if (!executor)
            executor_ = concore::spawn_executor{};
    }

    //! Called whenever this task is done, to continue with the execution of the graph
    void on_cont(std::exception_ptr) {
        CONCORE_PROFILING_SCOPE_N("chained_task.on_cont");
        // Try to execute the next tasks
        for (auto& n : next_tasks_) {
            if (n.impl_->pred_count_-- == 1) {
                chained_task next(std::move(n)); // don't keep the ref here anymore
                detail::enqueue_next(executor_, task(next), except_fun_);
            }
        }
        next_tasks_.clear();
    }

    //! Set the continuation of the task, so that it executes the next tasks in the graph.
    //! If the task already has a continuation, that would be called first.
    void set_continuation() {
        auto inner_cont = task_fun_.get_continuation();
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
        task_fun_.set_continuation(std::move(cont));
    }
};

} // namespace detail

inline namespace v1 {

chained_task::chained_task(task t, any_executor executor)
    : impl_(std::make_shared<detail::chained_task_impl>(std::move(t), executor)) {}

void chained_task::operator()() {
    CONCORE_PROFILING_SCOPE_N("chained_task.()");
    assert(impl_->pred_count_.load() == 0);

    // Set a new continuation to the task to be executed, to trigger the next chained tasks
    impl_->set_continuation();

    // Execute the current task
    impl_->task_fun_();

    // Clear the continuation from our main task
    impl_->task_fun_.set_continuation({});
}

void chained_task::set_exception_handler(except_fun_t except_fun) {
    impl_->except_fun_ = std::move(except_fun);
}

chained_task::operator bool() const noexcept { return static_cast<bool>(impl_); }

void chained_task::clear_next() {
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
