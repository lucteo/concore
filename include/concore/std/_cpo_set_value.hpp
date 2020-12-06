#pragma once

#include <concore/detail/cxx_features.hpp>

namespace concore {
namespace std_execution {
inline namespace v1 {

// TODO: properly implement this, test it and document it
struct set_value_t;
template <typename R, typename... Vs>
void set_value(R&& r, Vs&&... vs) {
    ((R &&) r).set_value((Vs &&) vs...);
};

} // namespace v1
} // namespace std_execution
} // namespace concore