#pragma once

#include <concore/detail/cxx_features.hpp>

#if CONCORE_USE_CXX2020 && CONCORE_CPP_VERSION >= 20

#if CONCORE_CPP_COMPILER(gcc)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif

#include <concore/_gen/c++20/execution.hpp>

#if CONCORE_CPP_COMPILER(gcc)
#pragma GCC diagnostic pop
#endif

namespace  concore {
    using namespace concore::_p2300::execution;
}
#endif
