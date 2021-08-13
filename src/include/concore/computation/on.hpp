#pragma once

#include <concore/computation/computation.hpp>
#include <concore/detail/call_recv_continuation.hpp>

namespace concore {
namespace computation {
namespace detail {

template <typename R, typename E, typename ValT>
struct on_computation_receiver {
    R finalRecv_;
    E exec_;

    template <typename... Val>
    void set_value(Val... val) { // zero or one parameter
        // Encapsulate the call to the final receiver in a task continuation
        auto cont = concore::detail::call_recv_continuation<R, ValT>{
                (R &&) finalRecv_, (Val &&) val...};
        // Execute the task in the context of the given executor
        concore::execute(exec_, task{[] {}, {}, std::move(cont)});
    }
    void set_done() noexcept { concore::set_done((R &&) finalRecv_); }
    void set_error(std::exception_ptr e) noexcept { concore::set_error((R &&) finalRecv_, e); }
};

template <typename PrevComp, typename E>
struct on_computation {
    using value_type = typename PrevComp::value_type;

    on_computation(PrevComp prevComp, E exec)
        : prevComp_((PrevComp &&) prevComp)
        , exec_((E &&) exec) {}

    template <typename Recv>
    void run_with(Recv r) {
        static_assert(concore::receiver<Recv>, "given type is not a receiver");
        using recv_type = on_computation_receiver<Recv, E, value_type>;
        auto recv = recv_type{(Recv &&) r, (E &&) exec_};
        concore::computation::run_with((PrevComp &&) prevComp_, std::move(recv));
    }

private:
    PrevComp prevComp_;
    E exec_;
};

} // namespace detail

inline namespace v1 {

/**
 * @brief   Moves the computation to a different executor
 * @tparam  PrevComp    The type of the previous computation
 * @tparam  E           The type of executor to move the computation to
 * @param   prevComp    The previous computation happening before the switching of executors
 * @param   exec        The executor to move computation to
 * @return  A computation object
 * @details
 *
 * This moves the result of the previous computation to a different executor. It is typically used
 * whenever other computation parts need to happen which are chained on top of this.
 *
 * Whenever possible, only the calls to set_value() are moved to an executor. If there are errors
 * or if the computation is cancelled, then those notifications may not be moved to the given
 * executor.
 *
 * Post-conditions:
 * - the returned type models the `computation` concept
 * - the previous computation is always run
 * - the previous computation is run in the thread in which this computation is started
 * - whenever the previous computation finishes successfully, this will also notify success, but
 * will do it from inside the given executor.
 * - if the previous computation finished with error or it's cancelled, then this will forward the
 * error/cancellation
 * - the forwarding of the error/cancellation typically doesn't happen on the given executor
 * - if the executor throws, this will report the error
 * - if the executor cancels execution, this will report the cancellation
 *
 * @see     transform(), bind_error()
 */
template <typename PrevComp, typename E>
inline detail::on_computation<PrevComp, E> on(PrevComp prevComp, E exec) {
    return {(PrevComp &&) prevComp, (E &&) exec};
}

// TODO: pipe operator

} // namespace v1
} // namespace computation
} // namespace concore