#pragma once

#include <concore/detail/cxx_features.hpp>

#if CONCORE_USE_CXX2020 && CONCORE_CPP_VERSION >= 20
#include <concore/_gen/c++20/execution.hpp>

namespace  concore {
    namespace execution = concore::_p2300::execution;
}
#endif
