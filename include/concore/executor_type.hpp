#pragma once

#include "task.hpp"

namespace concore {

//! The generic type of an executor. An executor is an abstraction that takes tasks and executes
//! them at a given time, maybe with certain constraints.
using executor_t = std::function<void(task)>;

} // namespace concore