#pragma once

#include <concore/detail/cxx_features.hpp>
#include <concore/detail/extra_type_traits.hpp>

#include <concore/_cpo/_cpo_execute.hpp>
#include <concore/task.hpp>

#if CONCORE_CXX_HAS_CONCEPTS
#include <concepts>
#endif
#include <type_traits>

namespace concore {

namespace detail {

struct invocable_archetype {
    void operator()() & noexcept {}
};

#if CONCORE_CXX_HAS_CONCEPTS

template <typename T>
using ref_of = std::remove_cvref_t<T>&;

template <typename E, typename F>
concept executor_of_impl =                                     //
        (std::invocable<ref_of<F>>)                            //
        &&(std::constructible_from<std::remove_cvref_t<F>, F>) //
        &&(std::move_constructible<std::remove_cvref_t<F>>)    //
        // &&(std::copy_constructible<E>)                      //
        // &&(std::is_nothrow_copy_constructible_v<E>)         //
        &&(std::is_copy_constructible_v<E>) //
        &&(std::equality_comparable<E>)     //
        &&(requires(const E& e, F&& f) {    //
            concore::execute(e, (F &&) f);
        });

#else

template <typename E, typename F>
inline constexpr bool executor_of_impl                                               //
        = (std::is_constructible<concore::detail::remove_cvref_t<F>, F>::value)      //
          && (std::is_move_constructible<concore::detail::remove_cvref_t<F>>::value) //
          && (std::is_copy_constructible<E>::value)                                  //
          && (std::is_copy_constructible_v<E>)                                       //
          &&(detail::cpo_execute::has_execute<E, F>);

#endif

}; // namespace detail

inline namespace v1 {

using detail::invocable_archetype;

template <typename E>
CONCORE_CONCEPT_OR_BOOL(executor) = detail::executor_of_impl<E, detail::invocable_archetype>;

template <typename E, typename F>
CONCORE_CONCEPT_OR_BOOL(executor_of) = executor<E>&& detail::executor_of_impl<E, F>;

template <typename E>
CONCORE_CONCEPT_OR_BOOL(task_executor) =           //
        detail::executor_of_impl<E, concore::task> //
                && noexcept(concore::execute(CONCORE_DECLVAL(E), CONCORE_DECLVAL(concore::task)));

} // namespace v1

} // namespace concore
