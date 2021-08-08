/**
 * @file    as_operation.hpp
 * @brief   Definition of @ref concore::v1::as_operation "as_operation"
 *
 * @see     @ref concore::v1::as_operation "as_operation"
 */
#pragma once

#include <concore/detail/extra_type_traits.hpp>

#include <concore/as_invocable.hpp>
#include <concore/_concepts/_concepts_executor.hpp>
#include <concore/_concepts/_concepts_receiver.hpp>
#include <concore/_cpo/_cpo_execute.hpp>

#include <utility>

namespace concore {
inline namespace v1 {

/**
 * @brief   Wrapper that transforms an executor and a receiver into an operation
 *
 * @tparam  E The type of the executor; should model @ref executor
 * @tparam  R The type of the receiver
 *
 * @details
 *
 * This is a convenience wrapper to shortcut the usage of scheduler and sender.
 *
 * This types models the @ref operation_state concept
 *
 * @see operation_state, executor, receiver, as_invocable, as_sender
 */
template <typename E, typename R>
struct as_operation {
    //! The type of executor to be used (remove cvref)
    using executor_type = concore::detail::remove_cvref_t<E>;
    //! The type of receiver to be used (remove cvref)
    using receiver_type = concore::detail::remove_cvref_t<R>;

    //! Constructor
    as_operation(executor_type e, receiver_type r) noexcept
        : executor_(std::move(e))
        , receiver_(std::move(r)) {
#if CONCORE_CXX_HAS_CONCEPTS
        static_assert(executor<executor_type>, "Type needs to match executor concept");
        static_assert(receiver_of<receiver_type>, "Type needs to match receiver concept");
#endif
    }

    //! Starts the asynchronous operation
    void start() noexcept {
        auto f = as_invocable<receiver_type>(std::move(receiver_));
        try {
            concore::execute(std::move(executor_), std::move(f));
        } catch (...) {
            f.dismiss();
            concore::set_error(std::move(receiver_), std::current_exception());
        }
    }

private:
    //! The executor to be used to execute operations
    executor_type executor_;
    //! The receiver that receives notifications from execution
    receiver_type receiver_;
};

} // namespace v1
} // namespace concore
