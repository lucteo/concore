#pragma once

#include <concore/computation/computation.hpp>
#include <concore/task.hpp>
#include <concore/task_cancelled.hpp>

namespace concore {
namespace computation {
namespace detail {

struct empty_recv {
    template <typename... Val>
    void set_value(Val...) {}
    void set_done() noexcept {}
    void set_error(std::exception_ptr) noexcept {}
};

struct to_task_recv {
    task_continuation_function cont_;

    template <typename... Val>
    void set_value(Val... val) { // zero or one parameter
        cont_({});
    }
    void set_done() noexcept {
        try {
            throw task_cancelled{};
        } catch (...) {
            try {
                cont_(std::current_exception());
            } catch (...) {
            }
        }
    }
    void set_error(std::exception_ptr ex) noexcept {
        try {
            cont_(ex);
        } catch (...) {
        }
    }
};

} // namespace detail

inline namespace v1 {

/**
 * @brief   Transform a computation into a task
 * @tparam  Comp    The type of the computation to be transformed into a task
 * @param   comp    The computation to be transformed into a task
 * @param   grp     The group in which to create the task
 * @param   cont    The continuation to be called whenever the computation is done
 * @return  A task that runs the computation
 * @details
 *
 * This takes a computation and returns a task that executes the computation. The task will be
 * created with the given group (if non-empty). The given continuation functor will be called
 * whenever the computation is completed. Please note, however, that the computation will not be
 * directly attached to the task.
 *
 * If the computation yields a value, the task will drop it.
 *
 * Post-conditions:
 * - the returned task will run the computation whenever it's executed
 * - the continuation functor will be called whenever the computation finishes
 * - if the continuation succeeds, the continuation will be called with an empty exception ptr
 * - if the continuation finishes with an error, that error will be passed to the continuation
 * - if the continuation is cancelled, the continuation will be called with task_cancelled exception
 *
 * @see     from_task(), computation
 */
template <typename Comp>
inline task to_task(Comp comp, task_group grp = {}, task_continuation_function cont = {}) {
    static_assert(concore::computation::computation<Comp>, "given type is not a computation");
    if (cont) {
        auto task_fun = [comp = (Comp &&) comp, cont = std::move(cont)]() mutable {
            run_with((Comp &&) comp, detail::to_task_recv{std::move(cont)});
        };
        return {std::move(task_fun), std::move(grp)};
    } else {
        auto task_fun = [comp = (Comp &&) comp]() mutable {
            run_with((Comp &&) comp, detail::empty_recv{});
        };
        return {std::move(task_fun), std::move(grp)};
    }
}

} // namespace v1
} // namespace computation
} // namespace concore