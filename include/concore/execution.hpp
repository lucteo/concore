#pragma once

#include <concore/detail/cxx_features.hpp>

#include <exception>

#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_done.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <concore/_cpo/_cpo_execute.hpp>
#include <concore/_cpo/_cpo_connect.hpp>
#include <concore/_cpo/_cpo_start.hpp>
#include <concore/_cpo/_cpo_submit.hpp>
#include <concore/_cpo/_cpo_schedule.hpp>
#include <concore/_cpo/_cpo_bulk_execute.hpp>
#include <concore/_concepts/_concepts_executor.hpp>
#include <concore/_concepts/_concepts_receiver.hpp>
#include <concore/_concepts/_concepts_sender.hpp>
#include <concore/_concepts/_concept_sender_to.hpp>
#include <concore/_concepts/_concept_operation_state.hpp>
#include <concore/_concepts/_concept_scheduler.hpp>

namespace concore {
inline namespace v1 {

// Exception types:
struct receiver_invocation_error : std::runtime_error, std::nested_exception {
    receiver_invocation_error() noexcept
        : runtime_error("receiver_invocation_error")
        , nested_exception() {}
};

} // namespace v1
} // namespace concore