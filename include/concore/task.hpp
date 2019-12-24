#pragma once

#include <functional>

namespace concore {

//! A task. Just a function that takes no arguments and returns nothing.
//! This can be enqueued into an executor and executed at a later time, and/or with certain
//! constraints.
using task = std::function<void()>;

} // namespace concore