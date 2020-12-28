#pragma once

#include <concore/detail/cxx_features.hpp>

#include <exception>

#include "_cpo_set_value.hpp"
#include "_cpo_set_done.hpp"
#include "_cpo_set_error.hpp"
#include "_cpo_execute.hpp"
#include "_cpo_connect.hpp"
#include "_cpo_start.hpp"
#include "_cpo_submit.hpp"
#include "_cpo_schedule.hpp"
#include "_cpo_bulk_execute.hpp"
#include "_concepts_executor.hpp"
#include "_concepts_receiver.hpp"
#include "_concepts_sender.hpp"
#include "_concept_sender_to.hpp"
#include "_concept_operation_state.hpp"
#include "_concept_scheduler.hpp"
#include "_as_receiver.hpp"
#include "_as_invocable.hpp"
#include "_as_operation.hpp"
#include "_as_sender.hpp"

namespace concore {
namespace std_execution {
inline namespace v1 {

// Exception types:
struct receiver_invocation_error : std::runtime_error, std::nested_exception {
    receiver_invocation_error() noexcept
        : runtime_error("receiver_invocation_error")
        , nested_exception() {}
};

} // namespace v1
} // namespace std_execution
} // namespace concore