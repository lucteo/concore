#pragma once

#include <concore/detail/cxx_features.hpp>

#if CONCORE_CXX_HAS_CONCEPTS

#include <concore/_cpo/_cpo_schedule.hpp>

#include <concepts>
#include <type_traits>

namespace concore {

namespace detail {}; // namespace detail

inline namespace v1 {

template <typename S>
concept scheduler = std::move_constructible<std::remove_cvref_t<S>>&&
        std::equality_comparable<std::remove_cvref_t<S>>&& requires(S&& s) {
    concore::schedule((S &&) s);
};

} // namespace v1

} // namespace concore

#endif