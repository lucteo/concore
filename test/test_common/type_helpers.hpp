#pragma once

#include <concore/execution.hpp>

#if CONCORE_USE_CXX2020 && CONCORE_CPP_VERSION >= 20

template <typename T>
struct type_printer;

template <typename... Ts>
struct type_array {};

template <typename ExpectedValType, typename S>
inline void check_val_types(S snd) {
    using t = typename concore::sender_traits<S>::value_types<type_array, type_array>;
    static_assert(std::is_same<t, ExpectedValType>::value);
}

template <typename ExpectedValType, typename S>
inline void check_err_type(S snd) {
    using t = typename concore::sender_traits<S>::error_types<type_array>;
    static_assert(std::is_same<t, ExpectedValType>::value);
}

template <bool Expected, typename S>
inline void check_sends_done(S snd) {
    constexpr bool val = concore::sender_traits<S>::sends_done;
    static_assert(val == Expected);
}

#endif
