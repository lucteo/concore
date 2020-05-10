#pragma once

namespace concore {

inline namespace v1 { struct init_data; }

namespace detail {

class task_system;

/**
 * @brief      Getter for the task_system object that also ensures that the library is initialized.
 *
 * @param      config  The configuration to be used for the library; can be null.
 *
 * @return     The task system object to be used inside concore.
 *
 * This is used so that we can automatically initialize the library before its first use.
 */
task_system& get_task_system(const init_data* config = nullptr);

} // namespace detail
} // namespace concore