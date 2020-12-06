#pragma once

#include <concore/detail/cxx_features.hpp>

namespace concore {
namespace std_execution {
inline namespace v1 {

// TODO: properly implement this, test it and document it
struct submit_t;
template <typename S, typename R>
void submit(S&& s, R&& r) {
    ((S &&) s).submit((R &&) r);
};

} // namespace v1
} // namespace std_execution
} // namespace concore