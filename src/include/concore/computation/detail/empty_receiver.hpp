#pragma once

#include <exception>

namespace concore {
namespace computation {
namespace detail {

//! Receiver that doesn't do anything. Useful for computation without continuation.
struct empty_receiver {
    template <typename... Val>
    void set_value(Val...) {}
    void set_done() noexcept {}
    void set_error(std::exception_ptr) noexcept {}
};

} // namespace detail
} // namespace computation
} // namespace concore