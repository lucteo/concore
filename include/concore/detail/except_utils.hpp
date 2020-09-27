#pragma once

#include "concore/task_group.hpp"

#include <exception>

namespace concore {
namespace detail {

/**
 * @brief      Install a handler for propagating exceptions
 *
 * @param      exception_storage  Location where to store caught exceptions
 * @param      grp                The group we need to install the exception handler
 *
 * This can be used to forward exceptions. Call this to install an exception handler that stores the
 * caught exception in the given location. After the operation is done one can check if an exception
 * is thrown.
 * Whenever the exception is thrown this will also cancel other tasks in the group.
 *
 * Usage example:
 *      exception_ptr thrown_exception;
 *      install_except_propagation_handler(thrown_exception, grp);
 *
 *      // do work on grp
 *      // wait on grp to complete
 *
 *      if (thrown_exception)
 *          std::rethrow_exception(thrown_exception);
 */
inline void install_except_propagation_handler(
        std::exception_ptr& exception_storage, task_group& grp) {
    auto except_fun = [&exception_storage, &grp](std::exception_ptr ex) {
        if (!exception_storage) {
            // Store the exception, so that we can re-throw it later
            exception_storage = std::move(ex);
            // Cancel the remaining
            grp.cancel();
        }
    };
    grp.set_exception_handler(except_fun);
}
} // namespace detail
} // namespace concore