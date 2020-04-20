#pragma once

#include <functional>
#include <exception>

namespace concore {

/**
 * @brief      Type of function to be called for handling exceptions
 *
 * This defines the type of exception handler function used across concore. A handler of this type
 * will be called whenever an exception is thrown.
 */
using except_fun_t = std::function<void(std::exception_ptr)>;

} // namespace concore