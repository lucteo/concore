#include "concore/task_group.hpp"
#include "concore/detail/task_system.hpp"

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

    //! The except function to be called whenever an exception occurs on the corresponding tasks
    std::function<void(std::exception_ptr)> except_fun_;

    task_group_impl() = default;
    task_group_impl(const std::shared_ptr<task_group_impl>& parent)
        : parent_(parent) {}

    bool is_cancelled() const {
        return is_cancelled_.load(std::memory_order_acquire) ||
               (parent_ && parent_->is_cancelled());
    }
};

void task_group_access::on_starting_task(task_group& grp, const task& t) {
    g_current_task_group = grp;
}
void task_group_access::on_task_done(task_group& grp, const task& t) {
    g_current_task_group = task_group{};
}
void task_group_access::on_task_exception(
        task_group& grp, const task& t, std::exception_ptr ex) {
    g_current_task_group = task_group{};
    if (grp.impl_ && grp.impl_->except_fun_)
        grp.impl_->except_fun_(ex);
}

} // namespace detail

inline namespace v1 {

task_group::task_group()
    : impl_() {}
task_group::~task_group() {}

task_group task_group::create(const task_group& parent) {
    task_group res;
    res.impl_ = std::make_shared<detail::task_group_impl>(parent.impl_);
    return res;
}

void task_group::set_exception_handler(std::function<void(std::exception_ptr)> except_fun) {
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

bool task_group::is_cancelled() const {
    return impl_ && impl_->is_cancelled();
}

task_group task_group::current_task_group() { return detail::g_current_task_group; }

bool task_group::is_current_task_cancelled() {
    return detail::g_current_task_group.is_cancelled();
}


bool task_group::is_active() const { return !impl_ || !impl_.unique(); }

} // namespace v1

} // namespace concore