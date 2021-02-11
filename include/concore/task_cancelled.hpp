#pragma once

#include <stdexcept>

namespace concore {

inline namespace v1 {

//! Exception that indicates that a task is cancelled
struct task_cancelled : std::runtime_error {
    task_cancelled() noexcept
        : std::runtime_error("task cancelled") {}
};

} // namespace v1

} // namespace concore