#pragma once

#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <concore/_cpo/_cpo_set_done.hpp>

#include <exception>
#include <functional>

namespace concore {
namespace computation {
namespace detail {

template <typename ParamT, typename F>
struct transform_result {
    using type = std::invoke_result_t<F, ParamT>;
};

template <typename F>
struct transform_result<void, F> {
    using type = std::invoke_result_t<F>;
};

template <typename OutT, typename F, typename R>
struct transform_computation_receiver {
    R finalRecv_;
    F trFun_;

    template <typename... Val>
    void set_value(Val... val) { // zero or one parameter
        try {
            if constexpr (std::is_void_v<OutT>) {
                std::invoke((F&)trFun_, (Val &&) val...);
                concore::set_value((R &&) finalRecv_);
            } else {
                concore::set_value((R &&) finalRecv_, std::invoke((F&)trFun_, (Val &&) val...));
            }
        } catch (...) {
            concore::set_error((R &&) finalRecv_, std::current_exception());
        }
    }
    void set_done() noexcept { concore::set_done((R &&) finalRecv_); }
    void set_error(std::exception_ptr e) noexcept { concore::set_error((R &&) finalRecv_, e); }
};

template <typename PrevComp, typename F>
struct transform_computation {
    using value_type = typename transform_result<typename PrevComp::value_type, F>::type;

    transform_computation(PrevComp prevComp, F trFun)
        : prevComp_((PrevComp &&) prevComp)
        , trFun_((F &&) trFun) {}

    template <typename Recv>
    void run_with(Recv r) {
        static_assert(concore::receiver<Recv>, "given type is not a receiver");
        using recv_type = transform_computation_receiver<value_type, F, Recv>;
        auto recv = recv_type{(Recv &&) r, (F &&) trFun_};
        concore::computation::run_with((PrevComp &&) prevComp_, std::move(recv));
    }

private:
    PrevComp prevComp_;
    F trFun_;
};

} // namespace detail

inline namespace v1 {

/**
 * @brief   Returns a computation that transforms the result of a previous computation
 * @tparam  PrevComp    The type of the previous computation
 * @tparam  F           The type of functor used to transform the previous result
 * @param   prevComp    The previous computation that needs to be transformed
 * @param   trFun       The functor used to transform the result of the previous computation
 * @return  A computation object
 * @details
 *
 * This creates a computation object that, whenever run, will execute the previous computation, gets
 * its value and transforms it with the given function and yields the final result. This woks also
 * in the cases in which the previous computation yeids void, or if the transform function returns
 * void; in this cases, this computation can be used to represent "do this, then that" (where "that"
 * is represented as a functor).
 *
 * The transformation function will be called on the thread used by the previous computation to
 * yield its result.
 *
 * If the previous computation is cancelled, or has an error, then the transform functor will not be
 * called, and the done/error state will be just forwarded.
 *
 * Post-conditions:
 * - the returned type models the `computation` concept
 * - the previous computation is always run
 * - if the previous computation finishes successfully, then the transform function is called
 * - if both the previous computation succeeds and the transform functor doesn't throw, set_value()
 * is called on the final receiver
 * - if the transform functor throws (if executed), set_error() will be called
 * - if the previous computation is not successful, then the transform functor is not called
 * - if the previous computation is not successful, then the error/cancellation is forwarded to the
 * final receiver
 *
 * @see     bind(), bind_error()
 */
template <typename PrevComp, typename F>
inline detail::transform_computation<PrevComp, F> transform(PrevComp prevComp, F trFun) {
    return {(PrevComp &&) prevComp, (F &&) trFun};
}

// TODO: pipe operator

} // namespace v1
} // namespace computation
} // namespace concore