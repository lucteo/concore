#pragma once

#include <concore/detail/extra_type_traits.hpp>

#include "_as_invocable.hpp"
#include "_concepts_executor.hpp"
#include "_concepts_receiver.hpp"

#include <utility>

namespace concore {
namespace std_execution {
inline namespace v1 {

/**
 * @brief   Wrapper that transforms an executor and a receiver into an operation
 *
 * @tparam  E The type of the executor
 * @tparam  R The type of the receiver
 *
 * This is a convenience wrapper to shortcut the usage of scheduler and sender.
 *
 * @see as_invocable, as_sender
 */
template <CONCORE_CONCEPT_OR_TYPENAME(executor) E, CONCORE_CONCEPT_OR_TYPENAME(receiver_of) R>
struct as_operation {

    using executor_type = concore::detail::remove_cvref_t<E>;
    using receiver_type = concore::detail::remove_cvref_t<R>;

    as_operation(executor_type e, receiver_type r) noexcept
        : executor_(std::move(e))
        , receiver_(std::move(r)) {}

    void start() noexcept {
        try {
            concore::std_execution::execute(
                    std::move(executor_), as_invocable<receiver_type>(receiver_));
        } catch (...) {
            concore::std_execution::set_error(std::move(receiver_), std::current_exception());
        }
    }

private:
    executor_type executor_;
    receiver_type receiver_;
};

} // namespace v1
} // namespace std_execution
} // namespace concore
