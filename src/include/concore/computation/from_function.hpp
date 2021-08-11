#pragma once

#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <concore/_concepts/_concepts_receiver.hpp>

#include <exception>
#include <functional>

namespace concore {
namespace computation {
namespace detail {

template <typename F>
struct from_function_computation {
    using value_type = std::invoke_result_t<F>;

    from_function_computation(F fun)
        : fun_((F &&) fun) {}

    template <typename Recv>
    void run_with(Recv r) {
        static_assert(concore::receiver<Recv>, "given type is not a receiver");
        try {
            if constexpr (std::is_void_v<value_type>) {
                std::invoke((F&)fun_);
                concore::set_value((Recv &&) r);
            } else {
                concore::set_value((Recv &&) r, std::invoke((F&)fun_));
            }
        } catch (...) {
            concore::set_error((Recv &&) r, std::current_exception());
        }
    }

private:
    F fun_;
};

} // namespace detail

inline namespace v1 {

/**
 * @brief   Returns a computation that just calls the given function
 * @tparam  F   The type of function used for the computation
 * @param   fun The function to be called as part of this computation
 * @return  A computation object
 * @details
 *
 * This creates a computation object that just calls the given function with no parameters. If the
 * function returns a value then the computation will yield it. If the function throws, the
 * computation will report the error. This computation can never be cancelled.
 *
 * Post-conditions:
 * - the returned type models the `computation` concept
 * - the given function is always run
 * - if the function succeeds, the computation yields the result value
 * - if the function throws, the error will be reported to the final receiver
 * - the computation never cals set_done()
 *
 * @see     from_task(), just_value()
 */
template <typename F>
detail::from_function_computation<F> from_function(F fun) {
    return {(F &&) fun};
}

} // namespace v1
} // namespace computation
} // namespace concore