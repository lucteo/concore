#pragma once

namespace concore {

inline namespace v1 {
struct init_data;
}

namespace detail {

class exec_context;

/**
 * @brief      Getter for the exec_context object that also ensures that the library is initialized.
 *
 * @param      config  The configuration to be used for the library; can be null.
 *
 * @return     The task system object to be used inside concore.
 *
 * If we have an execution context set to the current thread, this function will return it.
 * Otherwise, it will return the global execution context for the library.
 *
 * This is used so that we can automatically initialize the library before its first use.
 */
exec_context& get_exec_context(const init_data* config = nullptr);

//! Sets the given execution context for the current thread
void set_context_in_current_thread(exec_context* ctx);

//! Returns the init_data object used to initialize the library
init_data get_current_init_data();

} // namespace detail
} // namespace concore