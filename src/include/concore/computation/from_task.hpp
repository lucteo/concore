#pragma once

#pragma once

#include <concore/task.hpp>
#include <concore/_cpo/_cpo_execute.hpp>
#include <concore/_concepts/_concepts_receiver.hpp>
#include <concore/detail/call_recv_continuation.hpp>

namespace concore {
namespace computation {
namespace detail {

template <typename Recv>
void set_recv_continuation(task& t, Recv&& r) {
    auto inner_cont = t.get_continuation();
    if (inner_cont) {
        t.set_continuation(concore::detail::call_prev_and_recv_continuation<Recv, void>{
                std::move(inner_cont), (Recv &&) r});
    } else {
        t.set_continuation(concore::detail::call_recv_continuation<Recv, void>{(Recv &&) r});
    }
}

struct from_task_computation {
    using value_type = void;

    explicit from_task_computation(task t)
        : task_(std::move(t)) {}

    template <typename Recv>
    void run_with(Recv r) {
        static_assert(concore::receiver<Recv>, "given type is not a receiver");
        set_recv_continuation(task_, (Recv &&) r);
        task_();
    }

private:
    task task_;
};

template <typename E>
struct from_task_computation_exec {
    using value_type = void;

    from_task_computation_exec(task t, E exec)
        : task_(std::move(t))
        , exec_((E &&) exec) {}

    template <typename Recv>
    void run_with(Recv r) {
        set_recv_continuation(task_, (Recv &&) r);

        concore::execute((E &&) exec_, std::move(task_));
    }

private:
    task task_;
    E exec_;
};

} // namespace detail

inline namespace v1 {

/**
 * @brief   Returns a computation that executes the given task
 * @tparam  E       The type of executor to use to execute the task
 * @param   t       The task to be executed
 * @param   exec    The executor to be used when executing the task (optional)
 * @return  A computation object
 * @details
 *
 * This creates a computation object that, whenever run, will execute the given task; if an executor
 * is given, the task will be used in the given executor. The receiver will be called accordingly to
 * the result presented at the continuation of the task.
 *
 * Note that the task might create subtasks and move the continuation to one of these subtasks. This
 * means that the continuation may be called asynchronously on a completely different threads, and
 * thus the receiver will be called in the same way as well.
 *
 * Post-conditions:
 * - the returned type models the `computation` concept
 * - if the task run with success, the computation will eventually call set_value() on the given
 * receiver
 * - if the task yields an error, the computation will call set_error() passing the exception
 * - if the task is cancelled, the computation will call set_done() on the receiver
 *
 * @see     task
 */
template <typename E>
detail::from_task_computation_exec<E> from_task(task t, E exec) {
    return detail::from_task_computation_exec<E>{std::move(t), (E &&) exec};
}

detail::from_task_computation from_task(task t) {
    return detail::from_task_computation{std::move(t)};
}

} // namespace v1
} // namespace computation
} // namespace concore