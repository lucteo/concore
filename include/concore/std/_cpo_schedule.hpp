#pragma once

#include <concore/detail/cxx_features.hpp>

namespace concore {
namespace std_execution {
inline namespace v1 {

// TODO: properly implement this, test it and document it
struct schedule_t;
template <typename S>
void schedule(S&& s) {
    ((S &&) s).schedule();
};

} // namespace v1
} // namespace std_execution
} // namespace concore