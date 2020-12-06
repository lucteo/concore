#pragma once

#include <concore/detail/cxx_features.hpp>

#if CONCORE_CXX_HAS_CONCEPTS

#include "_cpo_execute.hpp"
#include "_cpo_set_done.hpp"
#include "_cpo_set_value.hpp"
#include "_cpo_set_error.hpp"
#include "_cpo_connect.hpp"
#include "_cpo_schedule.hpp"

#include <concepts>
#include <type_traits>
#include <exception>

namespace concore {

namespace std_execution {

namespace detail {

struct invocable_archetype {
    void operator()() & noexcept {}
};

// clang-format off
template <typename E, typename F>
concept executor_of_impl
    =  std::invocable<std::remove_cvref_t<F>&>
    && std::constructible_from<std::remove_cvref_t<F>, F>
    && std::move_constructible<std::remove_cvref_t<F>>
    && std::copy_constructible<E>
    && std::is_nothrow_copy_constructible_v<E>
    && std::equality_comparable<E>
    && requires(const E& e, F&& f) {
        concore::std_execution::execute(e, (F &&) f);
    }
    ;
// clang-format on

template <template <template <typename...> class Tuple, template <typename...> class Variant> class>
struct has_value_types;

template <template <template <typename...> class> class Variant>
struct has_error_types;

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

// TODO: document & test
// clang-format off
template <typename T, typename E = std::exception_ptr>
concept receiver
    = std::move_constructible<std::remove_cvref_t<T>>
    && std::constructible_from<std::remove_cvref_t<T>, T>
    && requires(std::remove_cvref_t<T>&& t, E&& e) {
        { concore::std_execution::set_done(std::move(t)) } noexcept;
        { concore::std_execution::set_error(std::move(t), (E&&) e) } noexcept;
    }
    ;
// clang-format on

// TODO: document & test
// clang-format off
template <typename T, typename... An>
concept receiver_of
    = receiver<T>
    && std::move_constructible<std::remove_cvref_t<T>>
    && std::constructible_from<std::remove_cvref_t<T>, T>
    && requires(std::remove_cvref_t<T>&& t, An&&... an) {
        concore::std_execution::set_value(std::move(t), (An&&) an...);
    }
    ;
// clang-format on

// TODO: document & test
// clang-format off
template <typename OpState>
concept operation_state
    = std::destructible<OpState>
    && std::is_object_v<OpState>
    && requires(OpState& op) {
        // { concore::start(op) } noexcept;
        { op.start() } noexcept; // TODO
    }
    ;
// clang-format on

// TODO: document & test
// clang-format off
template <typename S>
concept sender
    = std::move_constructible<std::remove_cvref_t<S>>
    // && !requires(std::remove_cvref_t<S>&& s) {
    //     typename sender_traits<std::remove_cvref_t<S>>::__unspecialized; // TODO
    // }
    ;
// clang-format on

// TODO: document & test
// clang-format off
template <typename S, typename R>
concept sender_to
    = sender<S>
    && receiver<R>
    && requires(S&& s, R&& r) {
        concore::std_execution::connect((S&&) s, (R&&) r);
    }
    ;
// clang-format on

// TODO: document & test
// clang-format off
template <typename S>
concept typed_sender
    = sender<S>
    && requires {
        typename detail::has_value_types<S::template value_types>;
        typename detail::has_error_types<S::template error_types>;
        typename std::bool_constant<S::sends_done>;
    }
    ;
// clang-format on

// TODO: document & test
// clang-format off
template <typename S>
concept scheduler
    = std::move_constructible<std::remove_cvref_t<S>>
    && std::equality_comparable<std::remove_cvref_t<S>>
    && requires(S&& s) {
        concore::std_execution::schedule((S&&) s);
    }
    ;
// clang-format on

} // namespace v1

} // namespace std_execution
} // namespace concore

#endif