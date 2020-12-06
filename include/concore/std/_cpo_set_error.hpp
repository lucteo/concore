#pragma once

#include <concore/detail/cxx_features.hpp>

namespace concore {
namespace std_execution {
inline namespace v1 {

// TODO: properly implement this, test it and document it
struct set_error_t;
template <typename R, typename E>
void set_error(R&& r, E&& e) noexcept {
    ((R &&) r).set_error((E &&) e);
};

} // namespace v1
} // namespace std_execution
} // namespace concore