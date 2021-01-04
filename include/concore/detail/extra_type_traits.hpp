#pragma once

#include <concore/detail/cxx_features.hpp>
#include <type_traits>

namespace concore {

namespace detail {

#if CONCORE_CPP_VERSION >= 20

using std::remove_cvref_t;

#else

// Ensure that we have remove_cvref_t from C++20
template <typename T>
struct remove_cvref {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
};
template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

#endif

} // namespace detail
} // namespace concore
