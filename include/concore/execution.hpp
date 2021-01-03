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

#if DOXYGEN_BUILD
/**
 * @brief   Customization point object that can be used to set values to receivers
 *
 * @param   r       The receiver object that is signaled about sender's success
 * @param   vs...   The values sent by the sender
 *
 * This is called by a sender whenever the sender has finished work and produces some values. This
 * can be called even if the sender doesn't have any values to send to the receiver.
 *
 * The `Receiver` type should model concepts `receiver` and `receiver_of<Vs...>`.
 *
 * @see     set_done(), set_error()
 */
template <typename Receiver, typename... Vs>
void set_value(Receiver&& r, Vs&&... vs);

/**
 * @brief   Type to use for customization point for set_value
 *
 * This can be used for types that do not directly model the receiver concepts. One can define a
 * `tag_invoke` customization point to make the type be a receiver.
 *
 * For a type to be receiver, it needs to have the following customization points:
 *  - `tag_invoke(set_value_t, receiver, ...)`
 *  - `tag_invoke(set_done_t, receiver)`
 *  - `tag_invoke(set_error_t, receiver, err)`
 *
 * @see     set_value()
 */
struct set_value_t {};

/**
 * @brief   Customization point object that can be used to signal stop to receivers
 *
 * @param   r       The receiver object that is signaled about sender's stop signal
 *
 * This is called by a sender whenever the sender is stopped, and the execution of the task cannot
 * continue. When this is called, @ref set_value() is not called anymore.
 *
 * The `Receiver` type should model concept `receiver`.
 *
 * @see     set_value(), set_error()
 */
template <typename Receiver>
void set_done(Receiver&& r);

/**
 * @brief   Type to use for customization point for set_done
 *
 * This can be used for types that do not directly model the receiver concepts. One can define a
 * `tag_invoke` customization point to make the type be a receiver.
 *
 * For a type to be receiver, it needs to have the following customization points:
 *  - `tag_invoke(set_value_t, receiver, ...)`
 *  - `tag_invoke(set_done_t, receiver)`
 *  - `tag_invoke(set_error_t, receiver, err)`
 *
 * @see     set_done()
 */
struct set_done_t {};

/**
 * @brief   Customization point object that can be used to notify receivers of errors
 *
 * @param   r       The receiver object that is signaled about sender's error
 * @param   e       The error to be to the receiver
 *
 * This is called by a sender whenever the sender has an error to report to the sender. Sending an
 * error means that the sender is done processing; it will not call set_value() and set_done().
 *
 * The `Receiver` type should model concept `receiver<E>`.
 *
 * @see     set_value(), set_done()
 */
template <typename Receiver, typename Err>
void set_error(Receiver&& r, Err&& e);

/**
 * @brief   Type to use for customization point for set_error
 *
 * This can be used for types that do not directly model the receiver concepts. One can define a
 * `tag_invoke` customization point to make the type be a receiver.
 *
 * For a type to be receiver, it needs to have the following customization points:
 *  - `tag_invoke(set_value_t, receiver, ...)`
 *  - `tag_invoke(set_done_t, receiver)`
 *  - `tag_invoke(set_error_t, receiver, err)`
 *
 * @see     set_error()
 */
struct set_error_t {};

/**
 * @brief   Customization point object that can be used to execute work on executors
 *
 * @param   e   The executor object we are using for our execution
 * @param   f   The functor to be invoked
 *
 * This will tell the executor object to invoke the given functor, according to the rules defined in
 * the executor.
 *
 * The `Executor` type should model concept `executor_of<Ftor>`.
 */
template <typename Executor, typename Ftor>
void execute(Executor&& e, Ftor&& f);

/**
 * @brief   Type to use for customization point for execute
 *
 * This can be used for types that do not directly model the executor concepts. One can define a
 * `tag_invoke` customization point to make the type be an executor.
 *
 * For any given type `Ex`, and a functor type `Fn`, defining
 *      void tag_invoke(execute_t, Ex, Fn) {...}
 *
 * will make the `executor_of<Ex, Fn>` be true. that is, one can later call:
 * @code{.cpp}
 *      execute(ex, f);
 * @endcode
 * , where `ex` is an object of type `Ex`, and `f` is an object of type `Fn`.
 *
 * @see     execute()
 */
struct execute_t {};

/**
 * @brief   Connect a sender with a receiver, returning an async operation object
 *
 * @param   snd The sender object, that triggers the work
 * @param   rcv The receiver object that receives the results of the work
 *
 * The type of the `rcv` parameter must model the `receiver` concept.
 *
 * Usage example:
 * @code{.cpp}
 *      auto op = connect(snd, rcv);
 *      // later
 *      start(op);
 * @endcode
 */
template <typename Sender, typename Receiver>
auto connect(Sender&& s, Receiver&& r);

/**
 * @brief   Customization-point-object tag for connect
 *
 * To add support for connect to a type S, with the receiver R, one can define:
 *      template <typename R>
 *      void tag_invoke(connect_t, S, R);
 *
 * @see     connect()
 */
struct connect_t {};

/**
 * @brief   Customization point object that can be used to start asynchronous operations
 *
 * @param   o       The operation that should be started
 *
 * This is called whenever one needs to start an asynchronous operation.
 *
 * The `Oper` type should model concept `operation_state`.
 */
template <typename Oper>
void start(Oper&& o);

/**
 * @brief   Type to use for customization point for starting async operations
 *
 * This can be used for types that do not directly model the operation_state concept. One can define
 * a `tag_invoke` customization point to make the type be an operation_state.
 *
 * @see     start()
 */
struct start_t {};

/**
 * @brief   Submit work from a sender, by combining it with a receiver
 *
 * @param   snd The sender object, that triggers the work
 * @param   rcv The receiver object that receives the results of the work
 *
 * The `sender_to<Sender, Receiver>` concept must hold.
 *
 * If there is no `submit` customization point defined for the given `Sender` object (taking a
 * `Receiver` object), then this will fall back to calling `connect()`.
 *
 * Usage example:
 * @code{.cpp}
 *      submit(snd, rcv);
 * @endcode
 *
 * @see     connect()
 */
template <typename Sender, typename Receiver>
void submit(Sender&& s, Receiver&& r);

/**
 * @brief   Customization-point-object tag for submit
 *
 * To add support for submit to a type S, with the receiver R, one can define:
 * @code{.cpp}
 *      template <typename R>
 *      void tag_invoke(submit_t, S, R);
 * @endcode
 *
 * @see     submit()
 */
struct submit_t {};

/**
 * @brief   Transforms a scheduler (an execution context) into a single-shot sender
 *
 * @param   sched   The scheduler object
 *
 * Usage example:
 * @code{.cpp}
 *      sender auto snd = schedule(sched);
 * @endcode
 */
template <typename Scheduler>
auto schedule(Scheduler&& s);

/**
 * @brief   Customization-point-object tag for schedule
 *
 * To add support for schedule to a type S, one can define:
 * @code{.cpp}
 *      template <typename S>
 *      auto tag_invoke(schedule_t, S);
 * @endcode
 */
struct schedule_t {};

/**
 * @brief   Customization point object that can be used to bulk_execute work on executors
 *
 * @param   e   The executor object we are using for our execution
 * @param   f   The functor to be invoked
 * @param   n   The number of times we have to invoke the functor
 *
 * This will tell the executor object to invoke the given functor, according to the rules defined in
 * the executor.
 */
template <typename Executor, typename Ftor, typename Num>
void bulk_execute(Executor&& e, Ftor&& f, Num n);

/**
 * @brief   Customization-point-object tag for bulk_execute
 *
 * To add support for bulk_execute to a type T, one can define:
 * @code{.cpp}
 *      template <typename F, typename N>
 *      void tag_invoke(bulk_execute_t, T, F, N);
 * @endcode
 */
struct bulk_execute_t {};

/**
 * @brief   A type representing the archetype of an invocable object
 *
 * This essentially represents a 'void()' functor.
 */
struct invocable_archetype {
    void operator()() & noexcept {}
};

/**
 * @brief   Concept that defines an executor
 *
 * @tparam  E   The type that we want to model the concept
 *
 * An executor object is an object that can "execute" work. Given a functor compatible with @ref
 * invocable_archetype, the executor will be able to execute that function, in some specified
 * manner.
 *
 * Properties that a type needs to have in order for it to be an executor:
 *  - it's copy-constructible
 *  - the copy constructor is nothrow
 *  - it's equality-comparable
 *  - one can call 'execute(obj, invocable_archetype{})', where 'obj' is an object of the type
 *
 * To be able to call `execute` on an executor, the executor type must have one the following:
 *  - an inner method 'execute' that takes a functor
 *  - an associated 'execute' free function that takes the executor and a functor
 *  - an customization point `tag_invoke(execute_t, Ex, Fn)`
 *
 * @see     executor_of, execute_t, execute
 */
template <typename E>
concept executor;

/**
 * @brief Defines an executor that can execute a given functor type
 *
 * @tparam  E   The type that we want to model the concept
 * @tparam  F   The type functor that can be called by the executor
 *
 * This is similar to @ref executor, but instead of being capable of executing 'void()' functors,
 * this can execute functors of the given type 'F'
 *
 * @see     executor
 */
template <typename E, typename F>
concept executor_of;

/**
 * @brief Concept that defines a bare-bone receiver
 *
 * @tparam T The type being checked to see if it's a bare-bone receiver
 * @tparam E The type of errors that the receiver accepts; default std::exception_ptr
 *
 * A receiver represents the continuation of an asynchronous operation. An asynchronous operation
 * may complete with a (possibly empty) set of values, an error, or it may be canceled. A receiver
 * has three principal operations corresponding to the three ways an asynchronous operation may
 * complete: `set_value`, `set_error`, and `set_done`. These are collectively known as a receiver’s
 * _completion-signal operations_.
 *
 * The following constraints must hold with respect to receiver's completion-signal operations:
 *  - None of a receiver’s completion-signal operations shall be invoked before `concore::start` has
 * been called on the operation state object that was returned by `concore::connect` to connect that
 * receiver to a sender.
 *  - Once `concore::start` has been called on the operation state object, exactly one of the
 * receiver’s completion-signal operations shall complete non-exceptionally before the receiver is
 * destroyed.
 *  - If `concore::set_value` exits with an exception, it is still valid to call
 * `concore::set_error` or `concore::set_done` on the receiver.
 *
 * A bare-bone receiver is a receiver that only checks for the following CPOs:
 *  - set_done()
 *  - set_error(E)
 *
 * The set_value() CPO is ignored in a bare-bone receiver, as a receiver may have many ways to be
 * notified about the success of a sender.
 *
 * In addition to these, the type should be move constructible and copy constructible.
 *
 * @see receiver_of, set_done(), set_error()
 */
template <typename T, typename E = std::exception_ptr>
concept receiver;

/**
 * @brief Concept that defines a receiver of a particular kind
 *
 * @tparam T     The type being checked to see if it's a bare-bone receiver
 * @tparam Vs... The types of the values accepted by the receiver
 * @tparam E     The type of errors that the receiver accepts; default std::exception_ptr
 *
 * A receiver represents the continuation of an asynchronous operation. An asynchronous operation
 * may complete with a (possibly empty) set of values, an error, or it may be canceled. A receiver
 * has three principal operations corresponding to the three ways an asynchronous operation may
 * complete: `set_value`, `set_error`, and `set_done`. These are collectively known as a receiver’s
 * _completion-signal operations_.
 *
 * The following constraints must hold with respect to receiver's completion-signal operations:
 *  - None of a receiver’s completion-signal operations shall be invoked before `concore::start`
 * has been called on the operation state object that was returned by `concore::connect` to
 * connect that receiver to a sender.
 *  - Once `concore::start` has been called on the operation state object, exactly one of the
 * receiver’s completion-signal operations shall complete non-exceptionally before the receiver is
 * destroyed.
 *  - If `concore::set_value` exits with an exception, it is still valid to call
 * `concore::set_error` or `concore::set_done` on the receiver.
 *
 * This concept checks that all three CPOs are defined (as opposed to `receiver` who only checks
 * `set_done` and `set_error`).
 *
 * This is an extension of the `receiver` concept, but also requiring the set_value() CPO to be
 * present, for a given set of value types.
 *
 * @see receiver, set_value(), set_done(), set_error()
 */
template <typename T, typename E = std::exception_ptr, typename... Vs>
concept receiver_of;

/**
 * @brief   Concept that defines a sender
 *
 * @tparam  S   The type that is being checked to see if it's a sender
 *
 * A sender represents an asynchronous operation not yet scheduled for execution. A sender’s
 * responsibility is to fulfill the receiver contract to a connected receiver by delivering a
 * completion signal.
 *
 * A sender, once the asynchronous operation is started must successfully call exactly one of these
 * on the associated receiver:
 *  - `set_value()` in the case of success
 *  - `set_done()` if the operation was canceled
 *  - `set_error()` if an exception occurred during the operation, or while calling `set_value()`
 *
 * The sender starts working when submit(S, R) or start(connect(S, R)) is called passing the sender
 * object in. The sender should not execute any other work after calling one of the three completion
 * signals operations. The sender should not finish its work without calling one of these/
 *
 * A sender typically has a connect() method to connect to a receiver, but this is not mandatory. A
 * CPO can be provided instead for the connect() method.
 *
 * A sender typically exposes the type of the values it sets, and the type of errors it can
 * generate, but this is not mandatory.
 *
 * @see receiver, receiver_of, typed_sender, sender_to
 */
template <typename S>
concept sender;

/**
 * @brief   Concept that defines a typed sender
 *
 * @tparam  S   The type that is being checked to see if it's a typed sender
 *
 * This is just like the @ref sender concept, but it requires the type information; that is the
 * types that expose the types of values it sets to the receiver and the type of errors it can
 * generate.
 *
 * @see sender, sender_to
 */
template <typename S>
concept typed_sender;

/**
 * @brief   Concept that brings together a sender and a receiver
 *
 * @tparam  S   The type of sender that is assessed
 * @tparam  R   The type of receiver that the sender must conform to
 *
 * This concept extends the `sender` concept, and ensures that it can connect to the given receiver
 * type. It does that by checking if `concore::connect(S, R)` is valid.
 *
 * @see     sender, receiver
 */
template <typename S, typename R>
concept sender_to;

/**
 * @brief   Concept that defines an operation state
 *
 * @tparam  OpState The type that is being checked to see if it's a operation_state
 *
 * An object whose type satisfies `operation_state` represents the state of an asynchronous
 * operation. It is the result of calling `concore::connect` with a sender and a receiver.
 *
 * A compatible type must implement the start() CPO. In addition, any object of this type must be
 * destructible. Only object types model operation states.
 *
 * @see sender, receiver, connect()
 */
template <typename OpState>
concept operation_state;

/**
 * @brief   Concept that defines a scheduler
 *
 * @tparam  S   The type that is being checked to see if it's a scheduler
 *
 * A scheduler type allows a schedule() operation that creates a sender out of the scheduler. A
 * typical scheduler contains an execution context that will pass to the sender on its creation.
 *
 * The type that match this concept must be move and copy constructible and must also define the
 * schedule() CPO.
 *
 * @see sender
 */
template <typename S>
concept scheduler;

#endif

// Exception types:
struct receiver_invocation_error : std::runtime_error, std::nested_exception {
    receiver_invocation_error() noexcept
        : runtime_error("receiver_invocation_error")
        , nested_exception() {}
};

} // namespace v1
} // namespace concore