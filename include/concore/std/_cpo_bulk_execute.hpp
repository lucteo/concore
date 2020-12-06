#pragma once

#include <concore/detail/cxx_features.hpp>

namespace concore {
namespace std_execution {
inline namespace v1 {

// TODO: properly implement this, test it and document it
struct bulk_execute_t;
template <typename S, typename F, typename N>
void bulk_execute(S&& s, F&& f, N&& n) {
    ((S &&) s).bulk_execute((F &&) f, (N &&) n);
};

} // namespace v1
} // namespace std_execution
} // namespace concore