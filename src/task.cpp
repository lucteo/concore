#include "concore/task.hpp"
#include "concore/task_cancelled.hpp"

namespace concore {
namespace detail {

//! TLS pointer to the current task_group object.
//! This will be set and reset at each task execution.
thread_local task* g_current_task{nullptr};

void execute_task(task& t) noexcept {
    g_current_task = &t;
    const auto& grp = t.get_task_group();

    // If the task is canceled, don't do anything
    if (grp && grp.is_cancelled()) {
        t.cancelled();
        return;
    }

    try {
        task_group_access::on_starting_task(grp);
        t();
        task_group_access::on_task_done(grp);
    } catch (...) {
        task_group_access::on_task_exception(grp, std::current_exception());
    }
    g_current_task = nullptr;
}

} // namespace detail

inline namespace v1 {

void task::operator()() {
    try {
        fun_();
        if (cont_fun_)
            cont_fun_(std::exception_ptr{});
    } catch (...) {
        if (cont_fun_)
            cont_fun_(std::current_exception());
        throw;
    }
}
void task::cancelled() {
    if (cont_fun_)
        cont_fun_(std::make_exception_ptr(task_cancelled{}));
}

task* task::current_task() { return detail::g_current_task; }

} // namespace v1

} // namespace concore