#pragma once

#include "detail/exec_context_if.hpp"
#include "detail/library_data.hpp"
#include "detail/task_priority.hpp"
#include "task.hpp"

#include <utility>

namespace concore {

inline namespace v1 {
class task;
}

inline namespace v1 {

/**
 * @brief The default global executor type.
 *
 * This is an executor that passes the tasks directly to concore's task system. Whenever there is a
 * core available, the task is executed.
 *
 * Is is the default executor.
 *
 * The executor takes as constructor parameter the priority of the task to be used when enqueueing
 * the task.
 *
 * Two executor objects are equivalent if their priorities match.
 *
 * @see inline_executor, spawn_executor
 */
struct global_executor {

    //! The priority of the task to be used
    using priority = detail::task_priority; //! The type of the priority
    static constexpr auto prio_critical =
            detail::task_priority::critical;                       //! Critical-priority tasks
    static constexpr auto prio_high = detail::task_priority::high; //! High-priority tasks
    static constexpr auto prio_normal =
            detail::task_priority::normal;                       //! Tasks with normal priority
    static constexpr auto prio_low = detail::task_priority::low; //! Tasks with low priority
    static constexpr auto prio_background =
            detail::task_priority::background; //! Tasks with lowest possible priority

    explicit global_executor(priority prio = prio_normal)
        : prio_(prio) {}

    template <typename F>
    void execute(F&& f) const {
        do_enqueue(detail::get_exec_context(), task{std::forward<F>(f)}, prio_);
    }

    template <typename F>
    void operator()(F&& f) const {
        execute(std::forward<F>(f));
    }
    // TODO (now): Remove this

    friend inline bool operator==(global_executor l, global_executor r) {
        return l.prio_ == r.prio_;
    }
    friend inline bool operator!=(global_executor l, global_executor r) { return !(l == r); }

private:
    priority prio_;
};

} // namespace v1
} // namespace concore
