#pragma once

#include <concore/_cpo/_cpo_set_value.hpp>

namespace concore {
namespace computation {
namespace detail {

template <typename T>
struct just_value_computation {
    using value_type = T;

    explicit just_value_computation(T val)
        : val_((T &&) val) {}

    template <typename Recv>
    void run_with(Recv r) {
        concore::set_value((Recv &&) r, (T &&) val_);
    }

private:
    T val_;
};

template <>
struct just_value_computation<void> {
    using value_type = void;

    template <typename Recv>
    void run_with(Recv r) {
        concore::set_value((Recv &&) r);
    }
};

} // namespace detail

inline namespace v1 {

/**
 * @brief   Returns a computation that just yields the given value
 * @tparam  T   The type of the value to yield
 * @param   val The value to yield
 * @return  A computation object
 * @details
 *
 * This creates a computation object that, whenever run, will yield the value passed here.
 * It is useful for starting computation chains.
 *
 * Post-conditions:
 * - the returned type models the `computation` concept
 * - when run, the computation will call set_value() passing the value given here
 * - the computation will not throw if the value has noexcept copy/move ctors
 * - the computation will never call set_done()
 *
 * @see     just_void()
 */
template <typename T>
detail::just_value_computation<T> just_value(T val) {
    return detail::just_value_computation<T>{(T &&) val};
}

//! @overload
detail::just_value_computation<void> just_value() { return {}; }

/**
 * @brief   Returns a computation that just yields nothing
 * @return  A computation object
 * @details
 *
 * Similar to just_value(), but instead of yielding a value it will just yield nothing. That is, it
 * will call the receiver with no value.
 *
 * @see just_value()
 */
detail::just_value_computation<void> just_void() { return {}; }

} // namespace v1
} // namespace computation
} // namespace concore