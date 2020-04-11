#pragma once

#include "task.hpp"

namespace concore {

/**
 * @brief Generic executor type.
 * 
 * This is a type-erasure, allowing all executor types to be stored and manipulated.
 * 
 * An executor is an abstraction that takes a task and schedules it execution, typically at a later
 * time, and maybe with certain constraints
 * 
 * @see global_executor, immediate_executor, serializer
 */
using executor_t = std::function<void(task)>;

} // namespace concore