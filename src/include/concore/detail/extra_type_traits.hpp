#pragma once

#include <concore/detail/cxx_features.hpp>
#include <type_traits>

namespace concore {

namespace detail {

#if CONCORE_CPP_VERSION >= 20

using std::remove_cvref_t;
using std::type_identity;

#else

// Ensure that we have remove_cvref_t from C++20
template <typename T>
struct remove_cvref {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
};
template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

template <typename T>
struct type_identity {
    using type = T;
};

#endif

#if CONCORE_CXX_HAS_CONCEPTS
template <typename T>
concept moveable_value = std::is_move_constructible<remove_cvref_t<T>>::value &&
        std::is_constructible_v<remove_cvref_t<T>, T>;
#endif

// Ensure that we have std::invoke_result and std::invoke_result_t from C++17
#if CONCORE_CPP_VERSION >= 17
using std::invoke_result;
using std::invoke_result_t;
#else
template <typename F, typename... Args>
using invoke_result = result_of<F(Args...)>;
template <typename F, typename... Args>
using invoke_result_t = result_of_t<F(Args...)>;
#endif

} // namespace detail
} // namespace concore
