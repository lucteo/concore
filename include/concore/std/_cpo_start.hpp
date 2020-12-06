#pragma once

#include <concore/detail/cxx_features.hpp>

namespace concore {
namespace std_execution {
inline namespace v1 {

// TODO: properly implement this, test it and document it
struct start_t;
template <typename OpState>
void start(OpState& op) {
    op.start();
};

} // namespace v1
} // namespace std_execution
} // namespace concore