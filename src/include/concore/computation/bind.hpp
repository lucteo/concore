#pragma once

#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <concore/_cpo/_cpo_set_done.hpp>
#include <concore/computation/computation.hpp>

#include <exception>
#include <functional>

namespace concore {
namespace computation {
namespace detail {

template <typename ParamT, typename F>
struct bind_result {
    using type = typename std::invoke_result_t<F, ParamT>::value_type;
};

template <typename F>
struct bind_result<void, F> {
    using type = typename std::invoke_result_t<F>::value_type;
};

template <typename F, typename R>
struct bind_computation_receiver {
    R finalRecv_;
    F chainFun_;

    template <typename... Val>
    void set_value(Val... val) { // zero or one parameter
        try {
            // Invoke the function and get the next computation
            auto nextComputation = std::invoke((F&)chainFun_, (Val &&) val...);
            static_assert(concore::computation::computation<decltype(nextComputation)>,
                    "returned object is not a computation");
            // Run the second computation with the final receiver
            concore::computation::run_with(std::move(nextComputation), (R &&) finalRecv_);
        } catch (...) {
            // If the functor throws, report this as an error
            concore::set_error((R &&) finalRecv_, std::current_exception());
        }
    }
    void set_done() noexcept { concore::set_done((R &&) finalRecv_); }
    void set_error(std::exception_ptr e) noexcept { concore::set_error((R &&) finalRecv_, e); }
};

template <typename F, typename R, typename PrevCompValT>
struct bind_error_computation_receiver {
    R finalRecv_;
    F chainFun_;

    template <typename... Val>
    void set_value(Val... val) { // zero or one parameter
        try {
            // Just forward the value
            concore::set_value((R &&) finalRecv_, (Val &&) val...);
        } catch (...) {
            concore::set_error((R &&) finalRecv_, std::current_exception());
        }
    }
    void set_done() noexcept { concore::set_done((R &&) finalRecv_); }
    void set_error(std::exception_ptr e) noexcept {
        try {
            // Invoke the function and get the next computation
            auto nextComputation = std::invoke((F&)chainFun_, e);
            using next_type = decltype(nextComputation);
            static_assert(concore::computation::computation<next_type>,
                    "returned object is not a computation");
            static_assert(std::is_same_v<PrevCompValT, typename next_type::value_type>,
                    "Computation types do not match");
            // Run the second computation with the final receiver
            concore::computation::run_with(std::move(nextComputation), (R &&) finalRecv_);

        } catch (...) {
            concore::set_error((R &&) finalRecv_, std::current_exception());
        }
    }
};

template <typename PrevComp, typename F>
struct bind_computation {
    using value_type = typename bind_result<typename PrevComp::value_type, F>::type;

    bind_computation(PrevComp prevComp, F chainFun)
        : prevComp_((PrevComp &&) prevComp)
        , chainFun_((F &&) chainFun) {}

    template <typename Recv>
    void run_with(Recv r) {
        static_assert(concore::receiver<Recv>, "given type is not a receiver");
        using recv_type = bind_computation_receiver<F, Recv>;
        auto recv = recv_type{(Recv &&) r, (F &&) chainFun_};
        concore::computation::run_with((PrevComp &&) prevComp_, std::move(recv));
    }

private:
    PrevComp prevComp_;
    F chainFun_;
};

template <typename PrevComp, typename F>
struct bind_error_computation {
    using value_type = typename std::invoke_result_t<F, std::exception_ptr>::value_type;

    bind_error_computation(PrevComp prevComp, F chainFun)
        : prevComp_((PrevComp &&) prevComp)
        , chainFun_((F &&) chainFun) {}

    template <typename Recv>
    void run_with(Recv r) {
        static_assert(concore::receiver<Recv>, "given type is not a receiver");
        using recv_type = bind_error_computation_receiver<F, Recv, typename PrevComp::value_type>;
        auto recv = recv_type{(Recv &&) r, (F &&) chainFun_};
        concore::computation::run_with((PrevComp &&) prevComp_, std::move(recv));
    }

private:
    PrevComp prevComp_;
    F chainFun_;
};

struct create_bind_computation {
    template <CONCORE_CONCEPT_OR_TYPENAME(computation) Comp, typename F>
    auto operator()(Comp&& c, F&& f) {
        return bind_computation<concore::detail::remove_cvref_t<Comp>, F>{(Comp &&) c, (F &&) f};
    }
};
struct create_bind_error_computation {
    template <CONCORE_CONCEPT_OR_TYPENAME(computation) Comp, typename F>
    auto operator()(Comp&& c, F&& f) {
        return bind_error_computation<concore::detail::remove_cvref_t<Comp>, F>{
                (Comp &&) c, (F &&) f};
    }
};
} // namespace detail

inline namespace v1 {

/**
 * @brief   Applies "bind" monadic operation to chain two computations
 * @tparam  PrevComp    The type of the previous computation
 * @tparam  F           The type of functor used to generate the second computation
 * @param   prevComp    The previous computation we used to chain to
 * @param   chainFun    The functor used to generate the second computation (from the prev result)
 * @return  A computation object
 * @details
 *
 * This chains two computations. The first computation is given directly as the first argument. The
 * second computation is generated by calling the functor passed as the second argument. This
 * functor is called with the result of the previous computation, so that proper chaining is made.
 * The functor must return a computation object.
 *
 * The chaining works even if the first computation yields void; in this case the given function
 * should not have any parameter, and the computations are just executed one after the other.
 *
 * Starting this computation will start the previous computation then the computation returned by
 * the functor that will finally be using the receiver passed when starting the computation.
 *
 * The chain function will be called on the thread used by the previous computation to yield its
 * result.
 *
 * If the previous computation is cancelled, or has an error, then the transform functor will not be
 * called, and the done/error state will be just forwarded.
 *
 * The bind operation can be used to build more complex transformations on computations.
 * Example of how one can build a simple 'transform' computation algorithm with 'bind':
 * @code{.cpp}
 *      template <typename PrevComp, typename F>
 *      auto transform(PrevComp c, F f) {
 *          using interim_type = typename PrevComp::value_type;
 *          auto chainFun = [f] (interim_type val) {
 *              return just_value(f(val));
 *          };
 *          return bind(std::move(c), std::move(chainFun));
 *      }
 * @endcode
 *
 * The previous computation parameter might be missing and in this case this will return a wrapper
 * that can be used to pipe computation algorithms.
 *
 * Post-conditions:
 * - the returned type models the `computation` concept
 * - the previous computation is always run
 * - if the previous computation finishes successfully, then the chain function is called
 * - the computation returned by the function is run if the functor doesn't throw
 * - set_done is called if either the previous computation or the next computation is cancelled
 * - set_error is called if either of the two computations throws or the given functor throws
 * - if set_done nor set_error are not called then set_value is called on the final receiver
 *
 * @see     transform(), bind_error()
 */
template <CONCORE_CONCEPT_OR_TYPENAME(computation) PrevComp, typename F>
inline detail::bind_computation<PrevComp, F> bind(PrevComp prevComp, F chainFun) {
    return detail::create_bind_computation{}((PrevComp &&) prevComp, (F &&) chainFun);
}

//! @overload
template <typename F>
inline auto bind(F&& f) {
    return detail::make_algo_wrapper(detail::create_bind_computation{}, (F &&) f);
}

/**
 * @brief   Similar to bind, but takes care of the error flow
 * @tparam  PrevComp    The type of the previous computation
 * @tparam  F           The type of functor used to generate the second computation
 * @param   prevComp    The previous computation we used to chain to
 * @param   chainFun    The functor used to generate the second computation (from the prev error)
 * @return  A computation object
 * @details
 *
 * This chains two computations if the first one fails. The first computation is given directly as
 * the first argument. If, when run, this computation succeeds, this behaves as we would just run
 * this computation. If however, there was an error while executing the first computation, then this
 * allows us to continue with a different computation. It calls the given functor to generate a new
 * computation and runs this computation with the final receiver.
 *
 * The functor takes a std::exception_ptr as a parameter and must return a computation object.
 *
 * The functor must return a computation with the same value type as the first computation.
 *
 * The chain function will be called on the thread used by the previous computation to yield its
 * error.
 *
 * If the previous computation is cancelled, or succeeds, then the transform functor will not be
 * called. The cancellation and and the success value from the previous computation is forwarded.
 *
 * The previous computation parameter might be missing and in this case this will return a wrapper
 * that can be used to pipe computation algorithms.
 *
 * Post-conditions:
 * - the returned type models the `computation` concept
 * - the previous computation is always run
 * - if the previous computation finishes with error, then the chain function is called (passing in
 * the exception generated by the first computation)
 * - the computation returned by the function is run (if the functor doesn't throw)
 * - if the functor throws, then this will report the error to the final receiver
 * - set_done is called if either the previous computation or the next computation is cancelled
 * - set_value is called when the previous computation succeeds
 *
 * @see     bind(), transform()
 */
template <CONCORE_CONCEPT_OR_TYPENAME(computation) PrevComp, typename F>
inline auto bind_error(PrevComp prevComp, F chainFun) {
    return detail::create_bind_error_computation{}((PrevComp &&) prevComp, (F &&) chainFun);
}

//! @overload
template <typename F>
inline auto bind_error(F&& f) {
    return detail::make_algo_wrapper(detail::create_bind_error_computation{}, (F &&) f);
}

} // namespace v1
} // namespace computation
} // namespace concore