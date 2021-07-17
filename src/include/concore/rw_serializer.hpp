/**
 * @file    rw_serializer.hpp
 * @brief   Defines the @ref concore::v1::rw_serializer "rw_serializer" class
 *
 * @see     @ref concore::v1::rw_serializer "rw_serializer"
 */
#pragma once

#include "task.hpp"
#include "any_executor.hpp"
#include "except_fun_type.hpp"

#include <memory>

namespace concore {

inline namespace v1 {

/**
 * @brief      Similar to a serializer but allows two types of tasks: READ and WRITE tasks.
 *
 * This class is not an executor. It binds together two executors: one for *READ* tasks and one for
 * *WRITE* tasks. This class adds constraints between *READ* and *WRITE* threads.
 *
 * The *READ* tasks can be run in parallel with other *READ* tasks, but not with *WRITE* tasks. The
 * *WRITE* tasks cannot be run in parallel neither with *READ* nor with *WRITE* tasks.
 *
 * This class provides two methods to access to the two executors: @ref reader() and @ref writer().
 * The @ref reader() executor should be used to enqueue *READ* tasks while the @ref writer()
 * executor should be used to enqueue *WRITE* tasks.
 *
 * This implementation slightly favors the *WRITEs*: if there are multiple pending *WRITEs* and
 * multiple pending *READs*, this will execute all the *WRITEs* before executing the *READs*. The
 * rationale is twofold:
 *     - it's expected that the number of *WRITEs* is somehow smaller than the number of *READs*
 *     (otherwise a simple serializer would probably work too)
 *     - it's expected that the *READs* would want to *read* the latest data published by the
 *     *WRITEs*, so they are aiming to get the latest *WRITE*
 *
 * **Guarantees**:
 *  - no more than 1 *WRITE* task is executed at once
 *  - no *READ* task is executed in parallel with other *WRITE* task
 *  - the *WRITE* tasks are executed in the order of enqueueing
 *
 * @see        reader_type, writer_type, serializer, rw_serializer
 */
class rw_serializer {
    struct impl;
    //! Implementation detail shared by both reader and writer types
    std::shared_ptr<impl> impl_;

public:
    /**
     * @brief      The type of the executor used for *READ* tasks.
     *
     * Objects of this type will be created by @ref rw_serializer to allow enqueueing *READ* tasks
     */
    class reader_type {
        std::shared_ptr<impl> impl_;

    public:
        //! Constructor. Should only be called by @ref rw_serializer
        explicit reader_type(std::shared_ptr<impl> impl);

        /**
         * @brief      Enqueue a functor as a write operation in the RW serializer
         *
         * @details
         *
         * Depending on the state of the parent @ref rw_serializer object this will enqueue the task
         * immediately (if there are no *WRITE* tasks), or it will place it in a waiting list to be
         * executed later. The tasks on the waiting lists will be enqueued once there are no more
         * *WRITE* tasks.
         */
        template <typename F>
        void execute(F&& f) const {
            do_enqueue(task{std::forward<F>(f)});
        }

        /**
         * @brief      Enqueue a functor as a write operation in the RW serializer
         *
         * @details
         *
         * Depending on the state of the parent @ref rw_serializer object this will enqueue the task
         * immediately (if there are no *WRITE* tasks), or it will place it in a waiting list to be
         * executed later. The tasks on the waiting lists will be enqueued once there are no more
         * *WRITE* tasks.
         *
         * If the enqueuing throws, then the serializer remains valid (can enqueue and execute other
         * tasks). The exception will be passed to the continuation of the given task.
         */
        void execute(task t) const noexcept { do_enqueue_noexcept(std::move(t)); }

        //! Equality operator
        friend inline bool operator==(reader_type l, reader_type r) { return l.impl_ == r.impl_; }
        //! Inequality operator
        friend inline bool operator!=(reader_type l, reader_type r) { return !(l == r); }

    private:
        //! Implementation method for enqueueing a READ task
        void do_enqueue(task t) const;
        void do_enqueue_noexcept(task t) const noexcept;
    };

    /**
     * @brief      The type of the executor used for *WRITE* tasks.
     *
     * Objects of this type will be created by @ref rw_serializer to allow enqueueing *WRITE* tasks
     */
    class writer_type {
        std::shared_ptr<impl> impl_;

    public:
        //! Constructor. Should only be called by @ref rw_serializer
        explicit writer_type(std::shared_ptr<impl> impl);

        /**
         * @brief      Enqueue a functor as a write operation in the RW serializer
         *
         * @details
         *
         * Depending on the state of the parent @ref rw_serializer object this will enqueue the task
         * immediately (if there are no other tasks executing), or it will place it in a waiting
         * list to be executed later. The tasks on the waiting lists will be enqueued, in order, one
         * by one. No new *READ* tasks are executed while we have *WRITE* tasks in the waiting list.
         */
        template <typename F>
        void execute(F&& f) const {
            do_enqueue(task{std::forward<F>(f)});
        }

        /**
         * @brief      Enqueue a functor as a write operation in the RW serializer
         *
         * @details
         *
         * Depending on the state of the parent @ref rw_serializer object this will enqueue the task
         * immediately (if there are no other tasks executing), or it will place it in a waiting
         * list to be executed later. The tasks on the waiting lists will be enqueued, in order, one
         * by one. No new *READ* tasks are executed while we have *WRITE* tasks in the waiting list.
         *
         * If the enqueuing throws, then the serializer remains valid (can enqueue and execute other
         * tasks). The exception will be passed to the continuation of the given task.
         */
        void execute(task t) const noexcept { do_enqueue_noexcept(std::move(t)); }

        //! @copydoc execute()
        void operator()(task t) const { do_enqueue(std::move(t)); }

        //! Equality operator
        friend inline bool operator==(writer_type l, writer_type r) { return l.impl_ == r.impl_; }
        //! Inequality operator
        friend inline bool operator!=(writer_type l, writer_type r) { return !(l == r); }

    private:
        //! Implementation method for enqueueing WRITE tasks
        void do_enqueue(task t) const;
        void do_enqueue_noexcept(task t) const noexcept;
    };

    /**
     * @brief      Constructor
     *
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
    explicit rw_serializer(any_executor base_executor = {}, any_executor cont_executor = {});

    /**
     * @brief      Returns an executor to enqueue *READ* tasks.
     *
     * @return     The executor for *READ* types
     */
    reader_type reader() const { return reader_type(impl_); }
    /**
     * @brief      Returns an executor to enqueue *WRITE* tasks.
     *
     * @return     The executor for *WRITE* types
     */
    writer_type writer() const { return writer_type(impl_); }

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
};

} // namespace v1
} // namespace concore