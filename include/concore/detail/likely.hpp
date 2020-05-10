#pragma once

#include "concore/detail/platform.hpp"

#if CONCORE_CPP_VERSION >= 20

// clang-format off
#define CONCORE_IF_LIKELY(cond) if (cond) [[likely]]
#define CONCORE_IF_UNLIKELY(cond) if (cond) [[unlikely]]
// clang-format on

#elif CONCORE_CPP_COMPILER(gcc) || CONCORE_CPP_COMPILER(clang)

#define CONCORE_IF_LIKELY(cond) if (__builtin_expect((cond), 1))
#define CONCORE_IF_UNLIKELY(cond) if (__builtin_expect((cond), 0))

#else

#define CONCORE_IF_LIKELY(cond) if (cond)
#define CONCORE_IF_UNLIKELY(cond) if (cond)

#endif