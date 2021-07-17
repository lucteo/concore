/**
 * @file    n_serializer.hpp
 * @brief   Defines the @ref concore::v1::n_serializer "n_serializer" class
 *
 * @see     @ref concore::v1::n_serializer "n_serializer"
 */
#pragma once

#include "task.hpp"
#include "any_executor.hpp"
#include "except_fun_type.hpp"

#include <memory>

namespace concore {

inline namespace v1 {

/**
 * @brief      Executor type that allows max N tasks to be executed at a given time.
 *
 * If the main purpose of other executors is to define where and when tasks will be executed, the
 * purpose of this executor is to introduce constrains between the tasks enqueued into it.
 *
 * Given *M* tasks to be executed, this serializer ensures that there are no more than *N* tasks
 * executed in parallel. It serializes the executions of this task. After *N* tasks start executing
 * all other tasks enqueued into the serializer are put on hold. As soon as one task is completed a
 * new task is scheduled for execution.
 *
 * As this executor doesn't know to schedule tasks for executor it relies on one or two given
 * executors to do the scheduling. If a `base_executor` is given, this will be the one used to
 * schedule for execution of tasks whenever a new task is enqueued and the pool on on-hold tasks is
 * empty. E.g., whenever we enqueue the first time in the serializer. If this is not given, the
 * @ref global_executor will be used.
 *
 * If a `cont_executor` is given, this will be used to enqueue tasks after another task is finished;
 * i.e., enqueue the next task. If this is not given, the serializer will use the `base_executor` if
 * given, or @ref spawn_continuation_executor.
 *
 * An n_serializer in a concurrent system based on tasks is similar to semaphores for traditional
 * synchronization-based concurrent systems. However, using n_serializer objects will not block
 * threads, and if the application has enough other tasks, throughput doesn't decrease.
 *
 * **Guarantees**:
 *  - no more than *N* task is executed at once.
 *  - if N==1, behaves like the @ref serializer class.
 *
 * @see        serializer, rw_serializer, any_executor, global_executor, spawn_continuation_executor
 */
class n_serializer : public std::enable_shared_from_this<n_serializer> {
public:
    /**
     * @brief      Constructor
     *
     * @param      N              The maximum number of tasks allowed to be run in parallel
     * @param      base_executor  Executor to be used to enqueue new tasks
     * @param      cont_executor  Executor that enqueues follow-up tasks
     *
     * @details
     *
     * If `base_executor` is not given, @ref global_executor will be used.
     * If `cont_executor` is not given, it will use `base_executor` if given, otherwise it will use
     * @ref spawn_continuation_executor for enqueueing continuations.
     *
     * The first executor is used whenever new tasks are enqueued, and no task is in the wait list.
     * The second executor is used whenever a task is completed and we need to continue with the
     * enqueueing of another task. In this case, the default, @ref spawn_continuation_executor tends
     * to work better than @ref global_executor, as the next task is picked up immediately by the
     * current working thread, instead of going over the most general flow.
     *
     * @see        global_executor, spawn_continuation_executor
     */
    explicit n_serializer(int N, any_executor base_executor = {}, any_executor cont_executor = {});

    /**
     * @brief      Executes the given functor in the context of the N serializer.
     *
     * @param      f     The task functor to be enqueued in the serializer
     *
     * @details
     *
     * If there are no more than *N* tasks in the serializer, this task will be enqueued in the
     * `base_executor` given to the constructor (default is @ref global_executor). If there are
     * already enough other tasks in the serializer, the given task will be placed in a waiting
     * list. When all the previous tasks are executed, this task will also be enqueued for
     * execution.
     */
    template <typename F>
    void execute(F&& f) const {
        do_enqueue(task{std::forward<F>(f)});
    }

    /**
     * @brief      Executes the given functor in the context of the N serializer.
     *
     * @param      f     The task functor to be enqueued in the serializer
     *
     * @details
     *
     * If there are no more than *N* tasks in the serializer, this task will be enqueued in the
     * `base_executor` given to the constructor (default is @ref global_executor). If there are
     * already enough other tasks in the serializer, the given task will be placed in a waiting
     * list. When all the previous tasks are executed, this task will also be enqueued for
     * execution.
     *
     * If the enqueuing throws, then the serializer remains valid (can enqueue and execute other
     * tasks). The exception will be passed to the continuation of the given task.
     */
    void execute(task t) const noexcept { do_enqueue_noexcept(std::move(t)); }

    /**
     * @brief      Sets the exception handler for enqueueing tasks
     *
     * @param      except_fun  The function to be called whenever an exception occurs.
     *
     * @details
     *
     * The exception handler set here will be called whenever an exception is thrown while
     * enqueueing a follow-up task. It will not be called whenever the task itself throws an
     * exception; that will be handled by the exception handler set in the group of the task.
     *
     * Cannot be called in parallel with task enqueueing and execution.
     *
     * @see task_group::set_exception_handler
     */
    void set_exception_handler(except_fun_t except_fun);

    //! Equality operator
    friend inline bool operator==(n_serializer l, n_serializer r) { return l.impl_ == r.impl_; }
    //! Inequality operator
    friend inline bool operator!=(n_serializer l, n_serializer r) { return !(l == r); }

private:
    struct impl;
    //! The implementation object of this n_serializer.
    //! We need this to be shared pointer for lifetime issue, but also to be able to copy the
    //! serializer easily.
    std::shared_ptr<impl> impl_;

    //! Enqueues a task for execution
    void do_enqueue(task t) const;
    void do_enqueue_noexcept(task t) const noexcept;
};

} // namespace v1
} // namespace concore