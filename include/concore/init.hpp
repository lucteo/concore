#pragma once

#include <functional>
#include <stdexcept>

namespace concore {

inline namespace v1 {

/**
 * @brief      Configuration data for the concore library
 *
 * Store here all the parameters needed to be passed to concore when initializing. Any parameters
 * that are left unfilled will have reasonable defaults in concore.
 */
struct init_data {
    //! The number of workers we need to create in the task system; 0 = num core available
    int num_workers_{0};
    //! The number of extra slots we reserve for other threads to temporary join the tasks system
    int reserved_slots_{10};
    //! Function to be called at the start of each thread.
    //! Use this if you want to do things like setting thread priority, affinity, etc.
    std::function<void()> worker_start_fun_;
};

/**
 * @brief      Initializes the concore library.
 *
 * @param      config  The configuration to be passed to the library; optional.
 *
 * This will initialize the library, with the given parameters. If the library is already
 * initialized this will throw an @ref already_initialized exception.
 *
 * If this is not explicitly called the library will be initialized with default settings the first
 * time that a global task needs to be executed.
 *
 * @see        shutdown(), is_initialized(), already_initialized
 */
void init(const init_data& config = {});

/**
 * @brief      Exception thrown when attempting to initialize the library more than once.
 *
 * Thrown when manually initializing after the library was already initialized, either automatically
 * or by explicitly calling @ref init().
 *
 * @see init(), is_initialized()
 */
struct already_initialized : std::runtime_error {
    already_initialized()
        : runtime_error("already initialized") {}
};

/**
 * @brief      Determines if the library is initialized.
 *
 * @return     True if initialized, False otherwise.
 */
bool is_initialized();

/**
 * @brief      Shuts down the concore library.
 *
 * This can be called at the end of the program to stop the worker threads and cleanup the enqueued
 * tasks. In general, the library is destroyed automatically at the end, so this is not necessarily
 * needed. However, we might want to call this in unit tests to ensure that the library is in a
 * clean state for the next test.
 *
 * @warning    It is forbidden to shutdown the library while it's still in use. All tasks need to be
 *             finished executing, and no other objects/functions are accessed anymore.
 *
 * @warning    After shutdown, any call to the library except an initialization-specific call leads
 *             to undefined behavior.
 */
void shutdown();

} // namespace v1
} // namespace concore