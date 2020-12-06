#pragma once

#include <concore/detail/cxx_features.hpp>

namespace concore {
namespace std_execution {
inline namespace v1 {

// TODO: properly implement this, test it and document it
struct set_done_t;
template <typename R>
void set_done(R&& r) noexcept {
    ((R &&) r).set_done();
};

} // namespace v1
} // namespace std_execution
} // namespace concore