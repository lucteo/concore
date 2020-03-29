#include "concore/task_control.hpp"
#include "concore/detail/task_system.hpp"

#include <atomic>
#include <cassert>
#include <functional>

namespace concore {

namespace detail {

//! TLS pointer to the current task_control object.
//! This will be set and reset at each task execution.
thread_local task_control g_current_task_control{};

//! The data for a task_control object. Can be shared between multiple task_control values.
//! Also the tasks have a shared reference to this one. This means that it will be destroyed when
//! all the task_control and all its tasks are destructed.
struct task_control_impl : std::enable_shared_from_this<task_control_impl> {
    //! The parent of this task_control
    std::shared_ptr<task_control_impl> parent_;

    //! Set to true whenever we want to cancel all the tasks with a task_control
    std::atomic<bool> is_cancelled_{false};

    //! The except function to be called whenever an exception occurs on the corresponding tasks
    std::function<void(std::exception_ptr)> except_fun_;

    task_control_impl() = default;
    task_control_impl(const std::shared_ptr<task_control_impl>& parent)
        : parent_(parent) {}

    bool is_cancelled() const {
        return is_cancelled_.load(std::memory_order_acquire) ||
               (parent_ && parent_->is_cancelled());
    }
};

void task_control_access::on_starting_task(task_control& tc, const task& t) {
    g_current_task_control = tc;
}
void task_control_access::on_task_done(task_control& tc, const task& t) {
    g_current_task_control = task_control{};
}
void task_control_access::on_task_exception(
        task_control& tc, const task& t, std::exception_ptr ex) {
    g_current_task_control = task_control{};
    if (tc.impl_ && tc.impl_->except_fun_)
        tc.impl_->except_fun_(ex);
}

} // namespace detail

inline namespace v1 {

task_control::task_control()
    : impl_() {}
task_control::~task_control() {}

task_control task_control::create(const task_control& parent) {
    task_control res;
    res.impl_ = std::make_shared<detail::task_control_impl>(parent.impl_);
    return res;
}

void task_control::set_exception_handler(std::function<void(std::exception_ptr)> except_fun) {
    assert(impl_);
    impl_->except_fun_ = std::move(except_fun);
}
void task_control::cancel() {
    assert(impl_);
    impl_->is_cancelled_.store(true, std::memory_order_release);
}

void task_control::clear_cancel() {
    assert(impl_);
    impl_->is_cancelled_.store(false, std::memory_order_release);
}

bool task_control::is_cancelled() const {
    assert(impl_);
    return impl_->is_cancelled();
}

task_control task_control::current_task_control() { return detail::g_current_task_control; }

bool task_control::is_current_task_cancelled() {
    task_control cur_tc = current_task_control();
    return cur_tc.impl_ && cur_tc.impl_->is_cancelled();
}


bool task_control::is_active() const { return !impl_ || !impl_.unique(); }

} // namespace v1

} // namespace concore