#pragma once

#include <concore/detail/cxx_features.hpp>

namespace concore {
namespace std_execution {
inline namespace v1 {

// TODO: properly implement this, test it and document it
struct connect_t;
template <typename S, typename R>
void connect(S&& s, R&& r) {
    ((S &&) s).connect((R &&) r);
};

} // namespace v1
} // namespace std_execution
} // namespace concore