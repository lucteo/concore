#pragma once

#include <concore/detail/cxx_features.hpp>

#if CONCORE_CXX_HAS_CONCEPTS

#include <concore/_cpo/_cpo_execute.hpp>

#include <concepts>
#include <type_traits>

namespace concore {

namespace detail {

struct invocable_archetype {
    void operator()() & noexcept {}
};

template <typename E, typename F>
concept executor_of_impl = std::invocable<std::remove_cvref_t<F>&>&&
        std::constructible_from<std::remove_cvref_t<F>, F>&&
                std::move_constructible<std::remove_cvref_t<F>>&& std::copy_constructible<E>&&
                        // std::is_nothrow_copy_constructible_v<E>&&
                        std::is_copy_constructible_v<E>&& std::equality_comparable<E>&& requires(
                                const E& e, F&& f) {
    concore::execute(e, (F &&) f);
};

}; // namespace detail

inline namespace v1 {

/**
 * @brief   A type representing the archetype of an invocable object
 *
 * This essentially represents a 'void()' functor.
 */
using detail::invocable_archetype;

/**
 * @brief   Concept that defines an executor
 *
 * @tparam  E   The type that we want to model the concept
 *
 * Properties that a type needs to have in order for it to be an executor:
 *  - it's copy-constructible
 *  - the copy constructor is nothrow
 *  - it's equality-comparable
 *  - one can call 'execute(obj, invocable_archetype{})', where 'obj' is an object of the type
 *
 * To be able to call `execute` on an executor, the executor type must have one the following:
 *  - an inner method 'execute' that takes a functor
 *  - an associated 'execute' free function that takes the executor and a functor
 *  - an customization point `tag_invoke(execute_t, Ex, Fn)`
 *
 * @see     executor_of, execute_t, execute
 */
template <typename E>
concept executor = detail::executor_of_impl<E, detail::invocable_archetype>;

/**
 * @brief Defines an executor that can execute a given functor type
 *
 * @tparam  E   The type that we want to model the concept
 * @tparam  F   The type functor that can be called by the executor
 *
 * This is similar to @ref executor, but instead of being capable of executing 'void()' functors,
 * this can execute functors of the given type 'F'
 *
 * @see     executor
 */
template <typename E, typename F>
concept executor_of = executor<E>&& detail::executor_of_impl<E, F>;

} // namespace v1

} // namespace concore

#endif