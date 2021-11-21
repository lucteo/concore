/**
 * @file    thread_pool.hpp
 * @brief   Definition of @ref concore::v1::static_thread_pool "static_thread_pool"
 *
 * @see     @ref concore::v1::static_thread_pool "static_thread_pool"
 */
#pragma once

#include <concore/detail/cxx_features.hpp>
#if CONCORE_CXX_HAS_CONCEPTS
#include <concore/_concepts/_concept_scheduler.hpp>
#include <concore/_concepts/_concepts_receiver.hpp>
#endif
#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_done.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <concore/_cpo/_cpo_schedule.hpp>
#include <concore/_cpo/_cpo_start.hpp>
#include <concore/task.hpp>
#include <concore/detail/extra_type_traits.hpp>
#include <concore/task_cancelled.hpp>

#include <exception>
#include <type_traits>

namespace concore {

inline namespace v1 {
class task_group;
}

inline namespace v1 {
class static_thread_pool;
}

namespace detail {

struct pool_data;

//! Gets the task group associated with the given pool. Used for testing
task_group get_associated_group(const static_thread_pool& pool);

//! Enqueue a task functor into the task pool
void pool_enqueue(pool_data& pool, task_function&& t);
//! Enqueue a task functor into the task pool; returns false if the pool was stopped
bool pool_enqueue_check_cancel(pool_data& pool, task_function&& t);
//! Enqueue a task into the task pool
void pool_enqueue_noexcept(pool_data& pool, task&& t) noexcept;

//! Operation struct that models operation_state concept.
//! Returned when the user calls 'connect' on the thread pool sender.
template <typename Receiver>
struct pool_sender_op {

    pool_sender_op(pool_data* pool, Receiver&& r)
        : pool_(pool)
        , receiver_(std::move(r)) {}

    friend void tag_invoke(concore::start_t, pool_sender_op& self) noexcept {
        struct task_obj {
            Receiver receiver_;

            explicit task_obj(Receiver recv)
                : receiver_(std::move(recv)) {}

            void operator()() { concore::set_value(std::move(receiver_)); }
        };
        try {
            if (!pool_enqueue_check_cancel(*self.pool_, task_obj{std::move(self.receiver_)}))
                concore::set_done(std::move(self.receiver_));
        } catch (...) {
            concore::set_error(std::move(self.receiver_), std::current_exception());
        }
    }

private:
    pool_data* pool_{nullptr};
    Receiver receiver_;
};

//! The sender type exposed by the thread pool
class thread_pool_sender {
public:
    explicit thread_pool_sender(pool_data* impl) noexcept;
    thread_pool_sender(const thread_pool_sender& r) noexcept;
    thread_pool_sender& operator=(const thread_pool_sender& r) noexcept;
    thread_pool_sender(thread_pool_sender&& r) noexcept;
    thread_pool_sender& operator=(thread_pool_sender&& r) noexcept;
    ~thread_pool_sender() = default;

    /**
     * \brief   Checks if this thread is part of the thread pool
     *
     * Returns true if the current thread was created by the thread pool or was later on attached to
     * it.
     */
    bool running_in_this_thread() const noexcept;

    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    template <CONCORE_CONCEPT_TYPENAME(receiver_of) R>
    pool_sender_op<remove_cvref_t<R>> connect(R&& r) const {
        return {impl_, (R &&) r};
    }

private:
    //! The implementation data; use pimpl idiom.
    //! Parent object must be active for the lifetime of this object
    pool_data* impl_{nullptr};

    friend bool operator==(const thread_pool_sender& l, const thread_pool_sender& r) noexcept;
    friend bool operator!=(const thread_pool_sender& l, const thread_pool_sender& r) noexcept;
};

bool operator==(const thread_pool_sender& l, const thread_pool_sender& r) noexcept;
bool operator!=(const thread_pool_sender& l, const thread_pool_sender& r) noexcept;

//! The scheduler type exposed by the thread pool
class thread_pool_scheduler {
public:
    using sender_type = thread_pool_sender;

    explicit thread_pool_scheduler(pool_data* impl) noexcept;
    thread_pool_scheduler(const thread_pool_scheduler& r) noexcept;
    thread_pool_scheduler& operator=(const thread_pool_scheduler& r) noexcept;
    thread_pool_scheduler(thread_pool_scheduler&& r) noexcept;
    thread_pool_scheduler& operator=(thread_pool_scheduler&& r) noexcept;
    ~thread_pool_scheduler() = default;

    /**
     * \brief   Checks if this thread is part of the thread pool
     *
     * Returns true if the current thread was created by the thread pool or was later on attached to
     * it.
     */
    bool running_in_this_thread() const noexcept;

    /**
     * @brief Returns a sender object corresponding to this thread pool
     * @return The required sender object.
     *
     * The sender object can be used to send work on the thread pool.
     */
    friend thread_pool_sender tag_invoke(schedule_t, thread_pool_scheduler sched) noexcept {
        return thread_pool_sender(sched.impl_);
    }

private:
    //! The implementation data; use pimpl idiom.
    //! Parent object must be active for the lifetime of this object
    pool_data* impl_{nullptr};

    friend bool operator==(const thread_pool_scheduler& l, const thread_pool_scheduler& r) noexcept;
    friend bool operator!=(const thread_pool_scheduler& l, const thread_pool_scheduler& r) noexcept;
};

bool operator==(const thread_pool_scheduler& l, const thread_pool_scheduler& r) noexcept;
bool operator!=(const thread_pool_scheduler& l, const thread_pool_scheduler& r) noexcept;

class thread_pool_executor {
public:
    using shape_type = size_t;
    using index_type = size_t;

    explicit thread_pool_executor(pool_data* impl) noexcept;
    thread_pool_executor(const thread_pool_executor& r) noexcept = default;
    thread_pool_executor& operator=(const thread_pool_executor& r) noexcept = default;
    thread_pool_executor(thread_pool_executor&& r) noexcept = default;
    thread_pool_executor& operator=(thread_pool_executor&& r) noexcept = default;
    ~thread_pool_executor() = default;

    /**
     * \brief   Checks if this thread is part of the thread pool
     *
     * Returns true if the current thread was created by the thread pool or was later on attached to
     * it.
     */
    bool running_in_this_thread() const noexcept;

    /**
     * \brief   Executes the given functor in the current thread pool.
     *
     * \tparam  F   The type of the functor to execute
     * \param   f   The functor to be executed
     *
     * This will ensure that the execution follows the properties established for this object.
     *
     * If the function exits with an exception, std::terminate() will be called.
     */
    template <typename F>
    void execute(F&& f) const {
        pool_enqueue(*impl_, task_function{(F &&) f});
    }

    void execute(task t) const noexcept { pool_enqueue_noexcept(*impl_, std::move(t)); }

private:
    //! The implementation data; use pimpl idiom.
    //! Parent object must be active for the lifetime of this object
    pool_data* impl_;

    friend bool operator==(const thread_pool_executor& l, const thread_pool_executor& r) noexcept;
    friend bool operator!=(const thread_pool_executor& l, const thread_pool_executor& r) noexcept;
};

bool operator==(const thread_pool_executor& l, const thread_pool_executor& r) noexcept;
bool operator!=(const thread_pool_executor& l, const thread_pool_executor& r) noexcept;
} // namespace detail

inline namespace v1 {

/**
 * \brief   A pool of threads that can execute work.
 *
 * This is constructed with the number of threads that are needed in the pool. Once constructed,
 * these threads cannot be detached from the pool. The user is allowed to attach other threads to
 * the pool, but without any possibility of detaching them earlier than the destruction of the pool.
 * There is no automatic resizing of the pool.
 *
 * The user can manually signal the thread pool to stop processing items, and/or wait for the
 * existing work items to drain out.
 *
 * When destructing this object, the implementation ensures to wait on all the in-progress items.
 *
 * Any scheduler or executor objects that are created cannot exceed the lifetime of this object.
 *
 * Properties of the static_thread_pool executor:
 *      - blocking.always
 *      - relationship.fork
 *      - outstanding_work.untracked
 *      - bulk_guarantee.parallel
 *      - mapping.thread
 *
 * @see executor, scheduler, sender
 */
class static_thread_pool {
public:
    //! The type of scheduler that this object exposes
    using scheduler_type = detail::thread_pool_scheduler;
    //! The type of executor that this object exposes
    using executor_type = detail::thread_pool_executor;

    /**
     * \brief   Constructs a thread pool.
     *
     * \param   num_threads     The number of threads to statically create in the thread pool
     *
     * @details
     *
     * This thread pool will create the given number of "internal" threads. This number of threads
     * cannot be changed later on. In addition to these threads, the user might manually add other
     * threads in the pool by calling the \ref attach() method.
     *
     * \see     attach()
     */
    explicit static_thread_pool(std::size_t num_threads);

    //! Copy constructor is DISABLED
    static_thread_pool(const static_thread_pool&) = delete;
    //! Copy assignment is DISABLED
    static_thread_pool& operator=(const static_thread_pool&) = delete;

    //! Move constructor
    static_thread_pool(static_thread_pool&&) = default;
    //! Move assignment
    static_thread_pool& operator=(static_thread_pool&&) = default;

    /**
     * \brief   Destructor for the static pool.
     *
     * @details
     *
     * Ensures that all the tasks already in the pool are drained out before destructing the pool.
     * New tasks will not be executed anymore in the pool.
     *
     * This is equivalent to calling stop() and then wait().
     */
    ~static_thread_pool();

    /**
     * \brief   Attach the current thread to the thread pool.
     *
     * @details
     *
     * The thread that is calling this will temporary join this thread pool. The thread will behave
     * as if it was created during the constructor of this class. The thread will be released from
     * the pool (and return to the caller) whenever the \ref stop() and \ref wait() are releasing
     * the threads from this pool.
     *
     * If the thread pool is stopped, this will exit immediately without attaching the current
     * thread to the thread pool.
     *
     * \see     stop(), wait()
     */
    void attach();

    //! Alternative name for \ref attach()
    void join();

    /**
     * \brief   Signal all work to complete
     *
     * @details
     *
     * This will signal the thread pool to stop working as soon as possible. This will return
     * immediately without waiting on the worker threads to complete.
     *
     * After calling this, no new work will be taken by the thread pool.
     *
     * This will cause the threads attached to this pool to detach (after completing ongoing work).
     *
     * This is not thread-safe. Ensure that this is called from a single thread.
     */
    void stop();

    /**
     * \brief   Wait for all the threads attached to the thread pool to complete
     *
     * @details
     *
     * If not already stopped, it will signal the thread pool for completion. Calling just \ref
     * wait() is similar to calling \ref stop() and then \ref wait().
     *
     * If there are ongoing tasks in the pool that are still executing, this will block until all
     * these tasks are completed.
     *
     * After the call to this function, all threads are either terminated or detached from the
     * thread pool.
     *
     * \see     stop(), attach()
     */
    void wait();

    /**
     * \brief   Returns a scheduler that can be used to schedule work here.
     *
     * @details
     *
     * The returned scheduler object can be used to create sender objects that may be used to
     * schedule receiver objects to this thread pool.
     *
     * The returned object has the following properties:
     *  - execution::allocator
     *  - execution::allocator(std::allocator<void>())
     *
     * \see     executor()
     */
    scheduler_type scheduler() noexcept;

    /**
     * \brief   Returns an executor object that can add work to this thread pool.
     *
     * @details
     *
     * This returns an executor object that can be used to schedule function objects to be executed
     * by this thread pool.
     *
     * The returned object has the following properties:
     *  - execution::blocking.possibly
     *  - execution::relationship.fork
     *  - execution::outstanding_work.untracked
     *  - execution::allocator
     *  - execution::allocator(std::allocator<void>())
     *
     * \see     scheduler()
     */
    executor_type executor() noexcept;

private:
    //! The implementation data; use pimpl idiom
    std::unique_ptr<detail::pool_data> impl_;

    friend task_group detail::get_associated_group(const static_thread_pool& pool);
};

} // namespace v1

} // namespace concore