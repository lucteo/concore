#include "concore/task.hpp"
#include "concore/task_cancelled.hpp"

namespace concore {
namespace detail {

//! TLS pointer to the current task_group object.
//! This will be set and reset at each task execution.
thread_local task* g_current_task{nullptr};

} // namespace detail

inline namespace v1 {

void task::operator()() noexcept {
    // Mark this as the currently executing task
    detail::g_current_task = this;

    // If the task is canceled, just execute the continuation
    const task_group& grp = task_group_.get_task_group();
    if (grp && grp.is_cancelled()) {
        if (cont_fun_)
            cont_fun_(std::make_exception_ptr(task_cancelled{}));
        return;
    }

    try {
        detail::task_group_access::on_starting_task(grp);
        fun_();
        detail::task_group_access::on_task_done(grp);
        if (cont_fun_)
            cont_fun_(std::exception_ptr{});
    } catch (...) {
        detail::task_group_access::on_task_exception(grp, std::current_exception());
        if (cont_fun_)
            cont_fun_(std::current_exception());
    }
    // No task is currently executing now
    detail::g_current_task = nullptr;
}

task* task::current_task() { return detail::g_current_task; }

task_continuation_function exchange_cur_continuation(task_continuation_function&& new_cont) {
    auto* cur_task = task::current_task();
    if (cur_task) {
        auto old_cont = cur_task->get_continuation();
        cur_task->set_continuation(std::move(new_cont));
        return old_cont;
    }
    return std::move(new_cont);
}

} // namespace v1

} // namespace concore