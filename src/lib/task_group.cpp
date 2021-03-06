#include "concore/task_group.hpp"

#include <atomic>
#include <cassert>
#include <functional>

namespace concore {

namespace detail {

//! TLS pointer to the current task_group object.
//! This will be set and reset at each task execution.
thread_local task_group g_current_task_group{};

//! The data for a task_group object. Can be shared between multiple task_group values.
//! Also the tasks have a shared reference to this one. This means that it will be destroyed when
//! all the task_group and all its tasks are destructed.
struct task_group_impl : std::enable_shared_from_this<task_group_impl> {
    //! The parent of this task_group
    std::shared_ptr<task_group_impl> parent_;

    //! Set to true whenever we want to cancel all the tasks with a task_group
    std::atomic<bool> is_cancelled_{false};

    //! The number of tasks active in this group
    std::atomic<int> num_active_tasks_{0};

    //! The except function to be called whenever an exception occurs on the corresponding tasks
    std::function<void(std::exception_ptr)> except_fun_;

    task_group_impl() = default;
    explicit task_group_impl(std::shared_ptr<task_group_impl> parent)
        : parent_(std::move(parent)) {}

    bool is_cancelled() const {
        return is_cancelled_.load(std::memory_order_acquire) ||
               (parent_ && parent_->is_cancelled());
    }
};

void task_group_access::on_starting_task(const task_group& grp) { g_current_task_group = grp; }
void task_group_access::on_task_done(const task_group& grp) { g_current_task_group = task_group{}; }
void task_group_access::on_task_exception(const task_group& grp, std::exception_ptr ex) {
    g_current_task_group = task_group{};
    // Recurse up to find a group that has a exception handler fun
    // Stop when we find the first one
    auto pimpl = grp.impl_;
    while (pimpl) {
        if (pimpl->except_fun_) {
            pimpl->except_fun_(ex);
            return;
        }
        pimpl = pimpl->parent_;
    }
}

// cppcheck-suppress constParameter
void task_group_access::on_task_created(const task_group& grp) {
    // Increase the number of active tasks; recurse up
    auto pimpl = grp.impl_;
    while (pimpl) {
        pimpl->num_active_tasks_++;
        pimpl = pimpl->parent_;
    }
}
void task_group_access::on_task_destroyed(const task_group& grp) {
    // Decrease the number of tasks; recurse up
    auto pimpl = grp.impl_;
    while (pimpl) {
        pimpl->num_active_tasks_--;
        pimpl = pimpl->parent_;
    }
}

} // namespace detail

inline namespace v1 {

task_group::task_group() = default;
task_group::~task_group() = default;

task_group task_group::create(const task_group& parent) {
    task_group res;
    res.impl_ = std::make_shared<detail::task_group_impl>(parent.impl_);
    return res;
}

void task_group::set_exception_handler(except_fun_t except_fun) {
    assert(impl_);
    impl_->except_fun_ = std::move(except_fun);
}
void task_group::cancel() {
    assert(impl_);
    impl_->is_cancelled_.store(true, std::memory_order_release);
}

void task_group::clear_cancel() {
    assert(impl_);
    impl_->is_cancelled_.store(false, std::memory_order_release);
}

bool task_group::is_cancelled() const { return impl_ && impl_->is_cancelled(); }

task_group task_group::current_task_group() { return detail::g_current_task_group; }

bool task_group::is_current_task_cancelled() { return detail::g_current_task_group.is_cancelled(); }

bool task_group::is_active() const { return impl_ && impl_->num_active_tasks_ > 0; }

task_group task_group::set_current_task_group(const task_group& grp) {
    task_group res = detail::g_current_task_group;
    detail::g_current_task_group = grp;
    return res;
}

} // namespace v1

} // namespace concore