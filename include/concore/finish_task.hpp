/**
 * @file    finish_task.hpp
 * @brief   Definitions of @ref concore::v1::finish_event "finish_event", @ref
 *          concore::v1::finish_task "finish_task" and @ref concore::v1::finish_wait "finish_wait"
 *
 * @see     @ref concore::v1::finish_event "finish_event", @ref concore::v1::finish_task
 *          "finish_task" and @ref concore::v1::finish_wait "finish_wait"
 */
#pragma once

#include "concore/task.hpp"
#include "concore/any_executor.hpp"
#include "concore/spawn.hpp"
#include "concore/inline_executor.hpp"

#include <atomic>
#include <memory>
#include <cassert>

namespace concore {

namespace detail {

//! Data needed for a finish event; used both for finish_task and finish_wait.
struct finish_event_impl {
    task task_;
    any_executor executor_;
    std::atomic<int> ref_count_;

    finish_event_impl(task t, any_executor e, int cnt)
        : task_(std::move(t))
        , executor_(std::move(e))
        , ref_count_(cnt) {
        assert(cnt >= 0);
    }
    ~finish_event_impl() = default;

    finish_event_impl(finish_event_impl&&) = delete;
    finish_event_impl& operator=(finish_event_impl&&) = delete;

    finish_event_impl(const finish_event_impl&) = delete;
    finish_event_impl& operator=(const finish_event_impl&) = delete;
};
} // namespace detail

inline namespace v1 {

struct finish_task;
struct finish_wait;

/**
 * @brief      A finish event
 *
 * This can be passed to various tasks that would notify this whenever the task is complete.
 * Depending on how the event is configured, after certain number of tasks are completed this
 * triggers an event. This can be used to join the execution of multiple tasks that can run in
 * parallel.
 *
 * The @ref notify_done() function must be called whenever the task is done.
 *
 * This is created via finish_task and finish_wait.
 *
 * Once a finish even is triggered, it cannot be reused anymore.
 *
 * @see finish_task, finish_wait
 */
struct finish_event {
    /**
     * @brief Returns a continuation function that will signal this event when executed.
     * @return The continuation function
     * @details
     *
     * This can be added to a task. When the continuation of the task will be executed, it would
     * have an effect like calling @ref notify_done(). If enough of these are called, the event is
     * triggered, which, depending on the construction will call a task or unblock a wait.
     *
     * This is considered another client of the event, so it will increase the counter of clients to
     * wait for.
     *
     * @see notify_done()
     */
    task_continuation_function get_continuation() const {
        assert(impl_);
        impl_->ref_count_++;
        auto pimpl = impl_;
        return [pimpl](std::exception_ptr) { on_notify_done(pimpl); };
    }

    /**
     * @brief      Called by other tasks to indicate their completion.
     *
     * @details
     *
     * When the right number of tasks have called this, then the event is trigger; that is,
     * executing a task or unblocking some wait call.
     *
     * In general, calling @ref get_continuation() is preferred, as it's more composable.
     *
     * @see get_continuation().
     */
    void notify_done() const {
        assert(impl_);
        on_notify_done(impl_);
    }

private:
    using impl_type = std::shared_ptr<detail::finish_event_impl>;
    //! Implementation details; shared between multiple objects of the same kind
    impl_type impl_;

    //! Users cannot construct this directly; it needs to be done through finish_task and
    //! finish_wait.
    explicit finish_event(std::shared_ptr<detail::finish_event_impl> impl)
        : impl_(std::move(impl)) {}

    friend finish_task;
    friend finish_wait;

    static void on_notify_done(const impl_type& impl) {
        if (impl->ref_count_-- == 1) {
            impl->executor_.execute(std::move(impl->task_));
        }
    }
};

/**
 * @brief      Abstraction that allows executing a task whenever multiple other tasks complete.
 *
 * This is created with a task (and an executor), and will execute the task (on the given executor)
 * whenever a certain set of previous tasks are executed.
 *
 * The recommended way to tie this to other tasks is through continuations. One can call the @ref
 * get_continuation() method to obtain a continuation that can be plugged into a task. If all the
 * continuations obtained from this object are called, then the task represented by this object is
 * called.
 *
 * Each time, a continuation functor is obtained, an inner counter is increased. Each time a
 * continuation is executed, the counter is decreased. Whenever the counter reaches zero as a result
 * of an operation, the task is enqueued.
 *
 * Alternatively, the counting and the triggering of the tasks can be done manually. Users can
 * specify an initial count on constructor, and can manually call `event().notify_done()`. If one
 * calls `event().notify_done()` enough time to correspond to the counter, the task will be
 * enqueued.
 *
 * With the help of this class one might implement a concurrent join: when a specific number of task
 * are done, then a continuation is triggered.
 *
 * This abstraction can also be used to implement simple continuation; it's just like a join, but
 * with only one task to wait on.
 *
 * After constructing a finish_task object, and connecting it to the predecessor tasks (either
 * through continuations or by passing the corresponding @ref finish_event to the tasks), this
 * object can be safely destructed. Its presence will not affect in any way the execution of the
 * task.
 *
 * Example usage:
 * @code {.cpp}
 *      concore::finish_task done_task([]{
 *          printf("done.");
 *          system_cleanup();
 *      });
 *      // Spawn 3 tasks
 *      concore::spawn(concore::task{[]{ do_work1(); }, {}, done_task.get_continuation()});
 *      concore::spawn(concore::task{[]{ do_work2(); }, {}, done_task.get_continuation()});
 *      concore::spawn(concore::task{[]{ do_work3(); }, {}, done_task.get_continuation()});
 *      // When they complete, the done task is triggered
 * @endcode
 *
 * @see finish_event, finish_wait
 */
struct finish_task {
    //! Constructor with a task and executor
    finish_task(task&& t, any_executor e, int initial_count = 0)
        : event_(std::make_shared<detail::finish_event_impl>(
                  std::move(t), std::move(e), initial_count)) {}
    //! Constructor with a task
    explicit finish_task(task&& t, int initial_count = 0)
        : event_(std::make_shared<detail::finish_event_impl>(
                  std::move(t), spawn_continuation_executor{}, initial_count)) {}
    //! Constructor with a functor and executor
    template <typename F>
    finish_task(F f, any_executor e, int initial_count = 0)
        : event_(std::make_shared<detail::finish_event_impl>(
                  task{std::forward<F>(f)}, std::move(e), initial_count)) {}
    //! Constructor with a functor
    template <typename F>
    explicit finish_task(F f, int initial_count = 0)
        : event_(std::make_shared<detail::finish_event_impl>(
                  task{std::forward<F>(f)}, spawn_continuation_executor{}, initial_count)) {}

    /**
     * @brief Get a continuation object to be used by a predecessor
     * @return The continuation functor to be called to trigger the done task
     * @details
     *
     * Whenever this is called, this will do two things:
     * - increment the internal counter of predecessors
     * - returns a continuation functor to be called by the predecessors
     *
     * If all the predecessors are executed, then this object will enqueue the current task to be
     * executed.
     *
     * Calling this function will increase the internal counter. Calling the returned function will
     * decrease it. When that reaches zero, the current task is enqueued.
     */
    task_continuation_function get_continuation() const { return event_.get_continuation(); }

    //! Getter for the finish_event object that should be distributed to other tasks.
    finish_event event() const { return event_; }

private:
    //! The event that triggers the execution of the task.
    //! The task to be executed is stored within the event itself.
    finish_event event_;
};

/**
 * @brief      Abstraction that allows waiting on multiple tasks to complete.
 *
 * This is similar to @ref finish_task, but instead of executing a task, this allows the user to
 * wait on all the tasks to complete. This wait, as expected, is a busy-way: other tasks are
 * executed while waiting for the finish event.
 *
 * Similar to @ref finish_task, this can also be used to implement concurrent joins. Instead of
 * spawning a task whenever the tasks are complete, this allows the current thread to wait on the
 * tasks to be executed.
 *
 * The preferred way to utilize this is with the help of continuations. One should call @ref
 * get_continuation() to obtain a function to be added to a predecessor task. Whenever all the
 * continuations functions returned by this are called, the waiting is done.
 *
 * Alternatively, one can pass an initial count of tasks to the constructor, and then calling
 * `event().notify_done()` for the given number of times to unblock the wait.
 *
 * Example usage:
 * @code
 *      concore::finish_wait done;
 *      // Spawn 3 tasks
 *      concore::spawn(concore::task{[]{ do_work1(); }, {}, done.get_continuation()});
 *      concore::spawn(concore::task{[]{ do_work2(); }, {}, done.get_continuation()});
 *      concore::spawn(concore::task{[]{ do_work3(); }, {}, done.get_continuation()});
 *
 *      // Wait for all 3 tasks to complete
 *      done.wait();
 * @endcode
 *
 * @see finish_event, finish_task
 */
struct finish_wait {
    //! Constructor
    explicit finish_wait(int initial_count = 0)
        : wait_grp_(task_group::create(task_group::current_task_group()))
        , event_(std::make_shared<detail::finish_event_impl>(
                  task{[] {}, wait_grp_}, inline_executor{}, initial_count)) {}

    /**
     * @brief Get a continuation object to be used by a predecessor
     * @return The continuation functor to be called to unblock the wait
     * @details
     *
     * Whenever this is called, this will do two things:
     * - increment the internal counter of predecessors
     * - returns a continuation functor to be called by the predecessors
     *
     * If all the predecessors are executed, then the busy-wait of this object will be ended.
     *
     * Calling this function will increase the internal counter. Calling the returned function will
     * decrease it. When that reaches zero, the busy-waiting will be ended.
     */
    task_continuation_function get_continuation() const { return event_.get_continuation(); }

    //! Getter for the finish_event object that should be distributed to other tasks.
    finish_event event() const { return event_; }

    /**
     * @brief      Wait for all the tasks to complete.
     *
     * @details
     *
     * This will wait for the right number of calls to the @ref finish_event::notify_done() function
     * on the exposed event. Until that, this will attempt to get some work from the system and
     * execute it. Whenever the right number of tasks are completed (i.e., the right amount of calls
     * to @ref finish_event::notify_done() are made), then this will be unblocked; it will return as
     * soon as possible.
     *
     * This can be called several times, but after the first time this is unblocked, the subsequent
     * calls will exit immediately.
     */
    void wait() { concore::wait(wait_grp_); }

private:
    //! The task group we are waiting on
    task_group wait_grp_;
    //! The event used to wait for the termination of tasks.
    finish_event event_;
};

} // namespace v1
} // namespace concore
