#pragma once

#include <concore/computation/computation.hpp>
#include <concore/computation/detail/empty_receiver.hpp>

namespace concore {
namespace computation {

inline namespace v1 {

/**
 * @brief   Runs the given computation
 * @tparam  Comp    The type of the computation to be run
 * @tparam  Recv    The type of receiver used to get completion notification (optional)
 * @param   comp    The computation to be run
 * @param   recv    The receiver that gets the completion notification (optional)
 * @details
 *
 * This is semantically equivalent with the `run_with` CPO. It runs a computation and sends the
 * completion notification to the given receiver. If no receiver is passed here, then there will be
 * no notification of completion.
 *
 *
 * Post-conditions:
 * - same as run_with
 *
 * @see     run_with(), run_on()
 */
template <typename Comp, typename Recv>
inline void run(Comp comp, Recv recv) {
    return run_with((Comp &&) comp, (Recv &&) recv);
}

//! @overload
template <typename Comp>
inline void run(Comp comp) {
    return run_with((Comp &&) comp, detail::empty_receiver{});
}

/**
 * @brief   Runs (starts) the given computation on the given executor
 * @tparam  Exec    The type of executor to run the computation from
 * @tparam  Comp    The type of the computation to be run
 * @tparam  Recv    The type of receiver used to get completion notification (optional)
 * @param   exec    The executor to run the computation on
 * @param   comp    The computation to be run
 * @param   recv    The receiver that gets the completion notification (optional)
 * @details
 *
 * Similar to run(), this will run the computation. The difference is that this will use the given
 * executor to start the computation run. Please note that the computation is started on the given
 * executor, but it may finish outside (e.g., if 'on()' was called while building the computation).
 *
 * If the executor cannot enqueue the running of the computation this function it will throw.
 *
 * Post-conditions:
 * - same as run_with, for executing the computation
 * - the computation is started on the given executor
 * - if the executor is cancelled or it throws, this function will also throw
 *
 * @see     run_with(), run()
 */
template <typename Exec, typename Comp, typename Recv>
inline void run_on(Exec exec, Comp comp, Recv recv) {
    auto task_fun = [comp = (Comp &&) comp, recv = (Recv &&) recv]() mutable {
        run_with((Comp &&) comp, (Recv &&) recv);
    };
    concore::execute((Exec &&) exec, task{std::move(task_fun)});
}

//! @overload
template <typename Exec, typename Comp>
inline void run_on(Exec exec, Comp comp) {
    auto task_fun = [comp = (Comp &&) comp]() mutable {
        run_with((Comp &&) comp, detail::empty_receiver{});
    };
    concore::execute((Exec &&) exec, std::move(task_fun));
}

} // namespace v1
} // namespace computation
} // namespace concore