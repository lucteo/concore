#pragma once

#include <concore/detail/cxx_features.hpp>

#include <exception>
#include <stdexcept>

#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_done.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <concore/_cpo/_cpo_execute.hpp>
#include <concore/_cpo/_cpo_connect.hpp>
#include <concore/_cpo/_cpo_start.hpp>
#include <concore/_cpo/_cpo_schedule.hpp>
#include <concore/_concepts/_concepts_executor.hpp>
#include <concore/_concepts/_concepts_receiver.hpp>
#include <concore/_concepts/_concepts_sender.hpp>
#include <concore/_concepts/_concept_sender_to.hpp>
#include <concore/_concepts/_concept_operation_state.hpp>
#include <concore/_concepts/_concept_scheduler.hpp>

namespace concore {
inline namespace v1 {

#if DOXYGEN_BUILD
/**
 * @file execution.hpp
 * @brief   Contains the main definitions to support C++23 executors proposal.
 *
 * @details
 *
 * Concepts defined:
 *      - @ref concore::v1::receiver
 *      - @ref concore::v1::receiver_of
 *      - @ref concore::v1::sender
 *      - @ref concore::v1::typed_sender
 *      - @ref concore::v1::sender_to
 *      - @ref concore::v1::operation_state
 *      - @ref concore::v1::scheduler
 *
 * Customization point object functors:
 *      - @ref concore::set_value()
 *      - @ref concore::set_done()
 *      - @ref concore::set_error()
 *      - @ref concore::connect()
 *      - @ref concore::start()
 *      - @ref concore::schedule()
 *
 * Customization point object tag types:
 *      - @ref concore::set_value_t
 *      - @ref concore::set_done_t
 *      - @ref concore::set_error_t
 *      - @ref concore::connect_t
 *      - @ref concore::start_t
 *      - @ref concore::schedule_t
 */

/**
 * @brief   Customization point object that can be used to set values to receivers
 *
 * @param   r   The receiver object that is signaled about sender's success
 * @param   vs  The values sent by the sender
 *
 * @details
 *
 * This call is equivalent to `tag_invoke(concore::set_value, r, vs...)`.
 *
 * This is called by a sender whenever the sender has finished work and produces some values. This
 * can be called even if the sender doesn't have any values to send to the receiver; i.e., it's a
 * void receiver.
 *
 * The `Receiver` type should model concepts @ref concore::v1::receiver "receiver" and
 * `receiver_of<Vs...>`.
 *
 * @see     set_done(), set_error(), receiver, receiver_of
 */
template <typename Receiver, typename... Vs>
void set_value(Receiver&& r, Vs&&... vs);

/**
 * @brief   Type to use for customization point for @ref concore::set_value()
 *
 * This is equivalent with `decltype(concore::set_value)` (if we ignore constness).
 *
 * To add support for @ref concore::set_value() to a type `T`, for value types `Vs...`, one needs to
 * define:
 * @code{.cpp}
 *      void tag_invoke(set_value_t, T, Vs...);
 * @endcode
 *
 * @see concore::set_value(), set_done_t, set_error_t
 */
struct set_value_t {};

/**
 * @brief   Customization point object that can be used to signal stop to receivers
 *
 * @param   r       The receiver object that is signaled about sender's stop signal
 *
 * @details
 *
 * This call is equivalent to `tag_invoke(concore::set_done, r)`.
 *
 * This is called by a sender whenever the sender is stopped, and the execution of the task cannot
 * continue. When this is called, @ref set_value() is not called anymore.
 *
 * The `Receiver` type should model concept @ref concore::v1::receiver "receiver".
 *
 * @see     set_value(), set_error(), receiver
 */
template <typename Receiver>
void set_done(Receiver&& r);

/**
 * @brief   Type to use for customization point for @ref concore::set_done()
 *
 * This is equivalent with `decltype(concore::set_done)` (if we ignore constness).
 *
 * To add support for @ref concore::set_done() to a type `T`, one needs to define:
 * @code{.cpp}
 *      void tag_invoke(set_done_t, T);
 * @endcode
 *
 * @see concore::set_done(), set_value_t, set_error_t
 */
struct set_done_t {};

/**
 * @brief   Customization point object that can be used to notify receivers of errors
 *
 * @param   r       The receiver object that is signaled about sender's error
 * @param   e       The error to be to the receiver
 *
 * @details
 *
 * This call is equivalent to `tag_invoke(concore::set_error, r, e)`.
 *
 * This is called by a sender whenever the sender has an error to report to the sender. Sending an
 * error means that the sender is done processing; it will not call @ref set_value() and @ref
 * set_done().
 *
 * The `Receiver` type should model concept `receiver<E>`.
 *
 * @see     set_value(), set_done(), receiver
 */
template <typename Receiver, typename Err>
void set_error(Receiver&& r, Err&& e);

/**
 * @brief   Type to use for customization point for @ref concore::set_error()
 *
 * This is equivalent with `decltype(concore::set_error)` (if we ignore constness).
 *
 * To add support for @ref concore::set_error() to a type `T`, with an error type `E`, one needs to
 * define:
 * @code{.cpp}
 *      void tag_invoke(set_error_t, T, E);
 * @endcode
 *
 * @see concore::set_error(), set_value_t, set_done_t
 */
struct set_error_t {};

/**
 * @brief   Connect a sender with a receiver, returning an async operation object
 *
 * @param   snd The sender object, that triggers the work
 * @param   rcv The receiver object that receives the results of the work
 *
 * @details
 *
 * The type of the `rcv` parameter must model the @ref concore::v1::receiver "receiver" concept.
 * Usually, the `snd` parameter will model the @ref concore::v1::sender "sender" and
 * `sender_to<Receiver>` concepts.
 *
 * The resulting type should model the @ref concore::v1::operation_state "operation_state" concept.
 *
 * Usage example:
 * @code{.cpp}
 *      auto op = concore::connect(snd, rcv);
 *      // later
 *      concore::start(op);
 * @endcode
 *
 * @see receiver, sender, sender_to, operation_state
 */
template <typename Sender, receiver Receiver>
auto connect(Sender&& snd, Receiver&& rcv);

/**
 * @brief   Customization point object tag for @ref concore::connect()
 *
 * To add support for @ref concore::connect() to a type `S`, with the receiver `R`, one can define:
 * @code{.cpp}
 *      void tag_invoke(connect_t, S, R);
 * @endcode
 *
 * @see     connect()
 */
struct connect_t {};

/**
 * @brief   Customization point object that can be used to start asynchronous operations
 *
 * @param   o       The operation that should be started
 *
 * @details
 *
 * This is called whenever one needs to start an asynchronous operation.
 *
 * The `Oper` type must model concept @ref concore::v1::operation_state "operation_state".
 *
 * @see operation_state
 */
template <operation_state Oper>
void start(Oper&& o);

/**
 * @brief   Customization point object tag for @ref concore::start()
 *
 * To add support for @ref concore::start() to a type `T`, one can define:
 * @code{.cpp}
 *      void tag_invoke(start_t, T);
 * @endcode
 *
 * @see     concore::start()
 */
struct start_t {};

/**
 * @brief   Transforms a scheduler (an execution context) into a single-shot sender
 *
 * @param   sched   The scheduler object
 *
 * @details
 *
 * The return type of the operation must model the @ref concore::v1::sender "sender" concept.
 *
 * The availability of this operation for a given type makes the type model the @ref
 * concore::v1::scheduler "scheduler" concept.
 *
 * Usage example:
 * @code{.cpp}
 *      sender auto snd = schedule(sched);
 * @endcode
 *
 * @see scheduler, sender
 */
template <typename Scheduler>
auto schedule(Scheduler&& sched);

/**
 * @brief   Customization point object tag for @ref concore::schedule()
 *
 * To add support for @ref concore::schedule() to a type `T`, one can define:
 * @code{.cpp}
 *      auto tag_invoke(schedule_t, T);
 * @endcode
 *
 * @see concore::schedule()
 */
struct schedule_t {};

/**
 * @brief Concept that defines a bare-bone receiver
 *
 * @tparam T The type being checked to see if it's a bare-bone receiver
 * @tparam E The type of errors that the receiver accepts; default ``std::exception_ptr``
 *
 * @details
 *
 * A receiver represents the continuation of an asynchronous operation. An asynchronous operation
 * may complete with a (possibly empty) set of values, an error, or it may be canceled. A receiver
 * has three principal operations corresponding to the three ways an asynchronous operation may
 * complete: @ref concore::set_value(), @ref concore::set_error(), and @ref concore::set_done().
 * These are collectively known as a receiver’s _completion-signal operations_.
 *
 * The following constraints must hold with respect to receiver's completion-signal operations:
 *  - None of a receiver’s completion-signal operations shall be invoked before @ref
 * concore::start() has been called on the operation state object that was returned by @ref
 * concore::connect() to connect that receiver to a sender.
 *  - Once @ref concore::start() has been called on the operation state object, exactly one of the
 * receiver’s completion-signal operations shall complete non-exceptionally before the receiver is
 * destroyed.
 *  - If @ref concore::set_value() exits with an exception, it is still valid to call
 * @ref concore::set_error() or @ref concore::set_done() on the receiver.
 *
 * A bare-bone receiver is a receiver that only checks for the following CPOs:
 *  - @ref set_done()
 *  - @ref set_error()
 *
 * The @ref set_value() CPO is ignored in a bare-bone receiver, as a receiver may have many ways to
 * be notified about the success of a sender.
 *
 * In addition to these, the type should be move constructible and copy constructible.
 *
 * @see receiver_of, set_done(), set_error()
 */
template <typename T, typename E = std::exception_ptr>
struct receiver {};

/**
 * @brief Concept that defines a receiver of a particular kind
 *
 * @tparam T     The type being checked to see if it's a bare-bone receiver
 * @tparam E     The type of errors that the receiver accepts; default ``std::exception_ptr``
 * @tparam Vs... The types of the values accepted by the receiver
 *
 * @details
 *
 * A receiver represents the continuation of an asynchronous operation. An asynchronous operation
 * may complete with a (possibly empty) set of values, an error, or it may be canceled. A receiver
 * has three principal operations corresponding to the three ways an asynchronous operation may
 * complete: @ref concore::set_value(), @ref concore::set_error(), and @ref concore::set_done().
 * These are collectively known as a receiver’s _completion-signal operations_.
 *
 * The following constraints must hold with respect to receiver's completion-signal operations:
 *  - None of a receiver’s completion-signal operations shall be invoked before @ref
 * concore::start() has been called on the operation state object that was returned by @ref
 * concore::connect() to connect that receiver to a sender.
 *  - Once @ref concore::start() has been called on the operation state object, exactly one of the
 * receiver’s completion-signal operations shall complete non-exceptionally before the receiver is
 * destroyed.
 *  - If @ref concore::set_value() exits with an exception, it is still valid to call
 * @ref concore::set_error() or @ref concore::set_done() on the receiver.
 *
 * This concept checks that all three CPOs are defined (as opposed to @ref receiver who only checks
 * @ref set_done() and @ref set_error()).
 *
 * This is an extension of the @ref receiver concept, but also requiring the @ref set_value() CPO to
 * be present, for a given set of value types.
 *
 * @see receiver, set_value(), set_done(), set_error()
 */
template <typename T, typename E = std::exception_ptr, typename... Vs>
struct receiver_of {};

/**
 * @brief   Concept that defines a sender
 *
 * @tparam  S   The type that is being checked to see if it's a sender
 *
 * @details
 *
 * A sender represents an asynchronous operation not yet scheduled for execution. A sender’s
 * responsibility is to fulfill the receiver contract to a connected receiver by delivering a
 * completion signal.
 *
 * A sender, once the asynchronous operation is started must successfully call exactly one of these
 * on the associated receiver:
 *  - @ref set_value() in the case of success
 *  - @ref set_done() if the operation was canceled
 *  - @ref set_error() if an exception occurred during the operation, or while calling @ref
 *    set_value()
 *
 * The sender starts working when ``start(connect(S, R))`` is called passing the
 * sender and a receiver object in. The sender should not execute any other work after calling one
 * of the three completion signals operations. The sender should not finish its work without calling
 * one of these.
 *
 * A sender should expose a @ref connect() customization-point object to connect it to a receiver.
 *
 * A sender typically exposes the type of the values it sets, and the type of errors it can
 * generate, but this is not mandatory.
 *
 * @see receiver, receiver_of, typed_sender, sender_to
 */
template <typename S>
struct sender {};

/**
 * @brief   Concept that defines a typed sender
 *
 * @tparam  S   The type that is being checked to see if it's a typed sender
 *
 * @details
 *
 * This is just like the @ref sender concept, but it requires the type information; that is the
 * types that expose the types of values it sets to the receiver and the type of errors it can
 * generate.
 *
 * @see sender, sender_to
 */
template <typename S>
struct typed_sender {};

/**
 * @brief   Concept that brings together a sender and a receiver
 *
 * @tparam  S   The type of sender that is assessed
 * @tparam  R   The type of receiver that the sender must conform to
 *
 * @details
 *
 * This concept extends the @ref sender concept, and ensures that it can connect to the given
 * receiver type. It does that by checking if @ref concore::connect() is valid for the given types.
 *
 * @see     sender, receiver
 */
template <typename S, typename R>
struct sender_to {};

/**
 * @brief   Concept that defines an operation state
 *
 * @tparam  OpState The type that is being checked to see if it's a operation_state
 *
 * @details
 *
 * An object whose type satisfies @ref operation_state represents the state of an asynchronous
 * operation. It is the result of calling @ref concore::connect() with a sender and a receiver.
 *
 * A compatible type must implement the @ref concore::start() CPO. In addition, any object of this
 * type must be destructible. Only object types model operation states.
 *
 * @see sender, receiver, connect()
 */
template <typename OpState>
struct operation_state {};

/**
 * @brief   Concept that defines a scheduler
 *
 * @tparam  S   The type that is being checked to see if it's a scheduler
 *
 * @details
 *
 * A scheduler type allows a @ref schedule() operation that creates a sender out of the scheduler. A
 * typical scheduler contains an execution context that will pass to the sender on its creation.
 *
 * The type that match this concept must be move and copy constructible and must also define the
 * @ref schedule() CPO.
 *
 * @see sender
 */
template <typename S>
struct scheduler {};

#endif

//! Exception type representing a receiver invocation error
struct receiver_invocation_error : std::runtime_error, std::nested_exception {
    //! Default constructor
    receiver_invocation_error() noexcept
        : runtime_error("receiver_invocation_error")
        , nested_exception() {}
};

} // namespace v1
} // namespace concore