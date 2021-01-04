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

using detail::invocable_archetype;

template <typename E>
concept executor = detail::executor_of_impl<E, detail::invocable_archetype>;

template <typename E, typename F>
concept executor_of = executor<E>&& detail::executor_of_impl<E, F>;

} // namespace v1

} // namespace concore

#endif