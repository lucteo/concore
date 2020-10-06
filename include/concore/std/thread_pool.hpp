#pragma once

#include <concore/detail/cxx_features.hpp>
#include <concore/task.hpp>

#include <exception>
#include <type_traits>

namespace concore {

inline namespace v1 {
class task_group;
}

namespace std_execution {

inline namespace v1 {
class static_thread_pool;
}

namespace detail {

struct pool_data;

//! Gets the task group associated with the given pool. Used for testing
task_group get_associated_group(const static_thread_pool& pool);

class thread_pool_sender {
public:
    explicit thread_pool_sender(pool_data* impl) noexcept;
    thread_pool_sender(const thread_pool_sender& r) noexcept;
    thread_pool_sender& operator=(const thread_pool_sender& r) noexcept;
    thread_pool_sender(thread_pool_sender&& r) noexcept;
    thread_pool_sender& operator=(thread_pool_sender&& r) noexcept;
    ~thread_pool_sender();

    // TODO:
    // see-below require(execution::blocking_t::never_t) const;
    // see-below require(execution::blocking_t::possibly_t) const;
    // see-below require(execution::blocking_t::always_t) const;
    // see-below require(execution::relationship_t::continuation_t) const;
    // see-below require(execution::relationship_t::fork_t) const;
    // see-below require(execution::outstanding_work_t::tracked_t) const;
    // see-below require(execution::outstanding_work_t::untracked_t) const;
    // see-below require(const execution::allocator_t<void>& a) const;
    // template<class ProtoAllocator>
    // see-below require(const execution::allocator_t<ProtoAllocator>& a) const;

    // static constexpr execution::bulk_guarantee_t query(execution::bulk_guarantee_t) const;
    // static constexpr execution::mapping_t query(execution::mapping_t) const;
    // execution::blocking_t query(execution::blocking_t) const;
    // execution::relationship_t query(execution::relationship_t) const;
    // execution::outstanding_work_t query(execution::outstanding_work_t) const;
    // see-below query(execution::context_t) const noexcept;
    // see-below query(execution::allocator_t<void>) const noexcept;
    // template<class ProtoAllocator>
    // see-below query(execution::allocator_t<ProtoAllocator>) const noexcept;

    /**
     * \brief   Checks if this thread is part of the thread pool
     *
     * Returns true if the current thread was created by the thread pool or was later on attached to
     * it.
     */
    bool running_in_this_thread() const noexcept;

    // template <template <class...> class Tuple, template <class...> class Variant>
    // using value_types = Variant<Tuple<>>;
    // template <template <class...> class Variant>
    // using error_types = Variant<exception_ptr>;
    // static constexpr bool sends_done = true;

    // TODO:
    // template <receiver_of R>
    // void connect(R&& r) const;

private:
    //! The implementation data; use pimpl idiom.
    //! Parent object must be active for the lifetime of this object
    pool_data* impl_{nullptr};

    friend bool operator==(const thread_pool_sender& l, const thread_pool_sender& r) noexcept;
    friend bool operator!=(const thread_pool_sender& l, const thread_pool_sender& r) noexcept;
};

bool operator==(const thread_pool_sender& l, const thread_pool_sender& r) noexcept;
bool operator!=(const thread_pool_sender& l, const thread_pool_sender& r) noexcept;

class thread_pool_scheduler {
public:
    using sender_type = thread_pool_sender;

    explicit thread_pool_scheduler(pool_data* impl) noexcept;
    thread_pool_scheduler(const thread_pool_scheduler& r) noexcept;
    thread_pool_scheduler& operator=(const thread_pool_scheduler& r) noexcept;
    thread_pool_scheduler(thread_pool_scheduler&& r) noexcept;
    thread_pool_scheduler& operator=(thread_pool_scheduler&& r) noexcept;
    ~thread_pool_scheduler() = default;

    // TODO:
    // see-below require(const execution::allocator_t<void>& a) const;
    // template<class ProtoAllocator>
    // see-below require(const execution::allocator_t<ProtoAllocator>& a) const;

    // see-below query(execution::context_t) const noexcept;
    // see-below query(execution::allocator_t<void>) const noexcept;
    // template<class ProtoAllocator>
    // see-below query(execution::allocator_t<ProtoAllocator>) const noexcept;

    /**
     * \brief   Checks if this thread is part of the thread pool
     *
     * Returns true if the current thread was created by the thread pool or was later on attached to
     * it.
     */
    bool running_in_this_thread() const noexcept;

    thread_pool_sender schedule() noexcept;

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

    // TODO:
    // see-below require(execution::blocking_t::never_t) const;
    // see-below require(execution::blocking_t::possibly_t) const;
    // see-below require(execution::blocking_t::always_t) const;
    // see-below require(execution::relationship_t::continuation_t) const;
    // see-below require(execution::relationship_t::fork_t) const;
    // see-below require(execution::outstanding_work_t::tracked_t) const;
    // see-below require(execution::outstanding_work_t::untracked_t) const;
    // see-below require(const execution::allocator_t<void>& a) const;
    // template<class ProtoAllocator>
    // see-below require(const execution::allocator_t<ProtoAllocator>& a) const;

    // TODO:
    // static constexpr execution::bulk_guarantee_t query(execution::bulk_guarantee_t::parallel_t)
    // const; static constexpr execution::mapping_t query(execution::mapping_t::thread_t) const;
    // execution::blocking_t query(execution::blocking_t) const;
    // execution::relationship_t query(execution::relationship_t) const;
    // execution::outstanding_work_t query(execution::outstanding_work_t) const;
    // see-below query(execution::context_t) const noexcept;
    // see-below query(execution::allocator_t<void>) const noexcept;
    // template<class ProtoAllocator>
    // see-below query(execution::allocator_t<ProtoAllocator>) const noexcept;

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
     *
     * \see     bulk_execute()
     */
    template <typename F>
    void execute(F&& f) const {
        internal_execute(task_function{f});
    }

    /**
     * \brief   Bulk-executes the given functor in the current thread pool.
     *
     * \tparam  F   The type of the functor to execute; compatible with void(size_t)
     * \param   f   The functor to be executed
     * \param   n   The number of calls to the function
     *
     * This will ensure that the execution follows the properties established for this object.
     *
     * If the function exits with an exception, std::terminate() will be called.
     *
     * \see     bulk_execute()
     */
    template <typename F>
    void bulk_execute(F&& f, size_t n) const {
        // TODO: this is suboptimal, better use conc_for and/or SIMD instructions
        // #prama simd
        for (size_t i = 0; i < n; i++)
            internal_execute([i, f]() { f(i); });
    }

private:
    //! Called by execute to start executing the given task function
    void internal_execute(task_function&& t) const;

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
 */
class static_thread_pool {
public:
    // TODO: document
    using scheduler_type = detail::thread_pool_scheduler;
    using executor_type = detail::thread_pool_executor;

    /**
     * \biref   Constructs a thread pool.
     *
     * \param   num_threads     The number of threads to statically create in the thread pool
     *
     * This thread pool will create the given number of "internal" threads. This number of threads
     * cannot be changed later on. In addition to these threads, the user might manually add other
     * threads in the pool by caling the \ref attach() method.
     *
     * \see     attach()
     */
    explicit static_thread_pool(std::size_t num_threads);

    // Copy is disabled
    static_thread_pool(const static_thread_pool&) = delete;
    static_thread_pool& operator=(const static_thread_pool&) = delete;

    static_thread_pool(static_thread_pool&&) = default;
    static_thread_pool& operator=(static_thread_pool&&) = default;

    /**
     * \brief   Destructor for the static pool.
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
     * The thread that is calling this will temporary join this thread pool. The thread will behav
     * as if it was created during the constructor of this class. The thread will be released from
     * the pool (and return to the caller) whenever the \ref stop() and \ref wait() are releasing
     * the theads from this pool.
     *
     * If the thread pool is stopped, this will exit immediatelly without attaching the current
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
     * This will signal the therad pool to stop working as soon as possible. This will return
     * immediatelly without waiting on the worker threads to complete.
     *
     * After calling this, no new work will be taken by the thread pool.
     *
     * This will cause the threads attached to this pool to detach (after completing ongoing work).
     */
    void stop();

    /**
     * \brief   Wait for all the threads attached to the thread pool to complete
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
     * The returned scheduler object can be used to create sender objects that may be used to submit
     * receiver objects to this thread pool.
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
     * This returns an executor object that can be used to submit function objects to be executed by
     * this thread pool.
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

} // namespace std_execution

} // namespace concore