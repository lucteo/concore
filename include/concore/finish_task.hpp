#pragma once

#include "concore/task.hpp"
#include "concore/executor_type.hpp"
#include "concore/spawn.hpp"
#include "concore/immediate_executor.hpp"

#include <atomic>
#include <memory>
#include <cassert>

namespace concore {

namespace detail {

//! Data needed for a finish event; used both for finish_task and finish_wait.
struct finish_event_impl {
    task task_;
    executor_t executor_;
    std::atomic<int> ref_count_;

    finish_event_impl(task t, executor_t e, int cnt)
        : task_(std::move(t))
        , executor_(std::move(e))
        , ref_count_(cnt) {
        assert(cnt > 0);
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
     * @brief      Called by other tasks to indicate their completion.
     *
     * When the right number of tasks have called this, then the event is trigger; that is,
     * executing a task or unblocking some wait call.
     */
    void notify_done() const {
        assert(impl_);
        if (impl_->ref_count_-- == 1) {
            impl_->executor_(std::move(impl_->task_));
        }
    }

private:
    //! Implementation details; shared between multiple objects of the same kind
    std::shared_ptr<detail::finish_event_impl> impl_;

    //! Users cannot construct this directly; it needs to be done through finish_task and
    //! finish_wait.
    explicit finish_event(std::shared_ptr<detail::finish_event_impl> impl)
        : impl_(std::move(impl)) {}

    friend finish_task;
    friend finish_wait;
};

/**
 * @brief      Abstraction that allows executing a task whenever multiple other tasks complete.
 *
 * This is created with a task (and an executor) and a count number. If the specific number of other
 * tasks will complete, then the given task is executed (in the given executor).
 *
 * With the help of this class one might implement a concurrent join: when a specific number of task
 * are done, then a continuation is triggered.
 *
 * This abstraction can also be used to implement simple continuation; it's just like a join, but
 * with only one task to wait on.
 *
 * This will expose a @ref finish_event object with the @ref event() function. Other tasks will call
 * @ref finish_event::notify_done() whenever they are completed. When the right amount of tasks call
 * it, then the task given at the constructor will be executed.
 *
 * After constructing a finish_task object, and passing the corresponding @ref finish_event to the
 * right amount of other tasks, this object can be safely destructed. Its presence will not affect
 * in any way the execution of the task.
 *
 * Example usage:
 *      concore::finish_task done_task([]{
 *          printf("done.");
 *          system_cleanup();
 *      }, 3);
 *      // Spawn 3 tasks
 *      auto event = done_task.event();
 *      concore::spawn([event]{
 *          do_work1();
 *          event.notify_done();
 *      });
 *      concore::spawn([event]{
 *          do_work2();
 *          event.notify_done();
 *      });
 *      concore::spawn([event]{
 *          do_work3();
 *          event.notify_done();
 *      });
 *      // When they complete, the first task is triggered
 *
 * @see finish_event, finish_wait
 */
struct finish_task {
    finish_task(task&& t, executor_t e, int count = 1)
        : event_(std::make_shared<detail::finish_event_impl>(std::move(t), std::move(e), count)) {}
    explicit finish_task(task&& t, int count = 1)
        : event_(std::make_shared<detail::finish_event_impl>(
                  std::move(t), spawn_continuation_executor, count)) {}

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
 * tasks to be executed
 *
 * This will expose a @ref finish_event object with the @ref event() function. Other tasks will call
 * @ref finish_event::notify_done() whenever they are completed. When the right amount of tasks call
 * it, then the @ref wait() method will be 'unblocked'.
 *
 *
 * Example usage:
 *      concore::finish_wait done(3);
 *      auto event = done_task.event();
 *      // Spawn 3 tasks
 *      concore::spawn([event]{
 *          do_work1();
 *          event.notify_done();
 *      });
 *      concore::spawn([event]{
 *          do_work2();
 *          event.notify_done();
 *      });
 *      concore::spawn([event]{
 *          do_work3();
 *          event.notify_done();
 *      });
 *
 *      // Wait for all 3 tasks to complete
 *      done.wait();
 *
 * @see finish_event, finish_task
 */
struct finish_wait {
    explicit finish_wait(int count = 1)
        : wait_grp_(task_group::create(task_group::current_task_group()))
        , event_(std::make_shared<detail::finish_event_impl>(
                  task{[] {}, wait_grp_}, immediate_executor, count)) {}

    //! Getter for the finish_event object that should be distributed to other tasks.
    finish_event event() const { return event_; }

    /**
     * @brief      Wait for all the tasks to complete.
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
