#pragma once

#include "concore/task_group.hpp"
#include "concore/spawn.hpp"
#include "concore/detail/partition_work.hpp"

namespace concore {

inline namespace v1 {

/**
 * @brief      Hint indicating the method of dividing the work within a conc_for
 *
 * Using this would provide a hint the conc_for algorithm on how to partition the input data.
 *
 * The implementation may not follow the given advice. Typically the default method (autp_partition)
 * works good enough, so most users don't need to change this.
 */
enum class for_method {
    /**
     * Automatically partitions the data, trying to maximize locality.
     *
     * This method tries to create as many tasks as needed to fill the available workers, but tries
     * not to split the work too much to reduce locality. If only one worker is free to do work,
     * this method tries to put all the iterations in the task without splitting the work.
     *
     * Whenever new workers can take tasks, this method tries to ensure that the furthest away
     * elements are taken from the current processing.
     *
     * This method tries as much as possible to keep all the available workers busy, hopefully to
     * finish this faster. It works well if different iterations take different amounts of time
     * (work is not balanced).
     *
     * It uses spawning to divide the work. Can be influenced by the granularity level.
     *
     * This is the default method for random-access iterators.
     */
    auto_partition,
    /**
     * Partitions the data upfront.
     *
     * Instead of partitioning the data on the fly lie the auto_partiion method, this will partition
     * the data upfront, creating as many tasks as needed to cover all the workers. This can
     * minimize the task management, but it doesn't necessarily to ensure that all the workers have
     * tasks to do, especially in unbalanced workloads.
     *
     * Locality is preserved when splitting upfront.
     *
     * This method only works for random-access iterators.
     */
    upfront_partition,
    /**
     * Partitions the iterations as it advances through them.
     *
     * This partition tries to create a task for each iteration (or, if granularity is > 1, for a
     * number of iterations), and the tasks are created as the algorithm progresses. Locality is not
     * preserved, as nearby elements typically end up on different threads. This method tries to
     * always have tasks to be executed. When a task finished, a new task is spawned.
     *
     * This method works for forward iterators.
     *
     * This is the default method for non-random-access iterators.
     */
    iterative_partition,
    /**
     * Naive partition.
     *
     * This creates a task for each iteration (or, depending of granularity, on each group of
     * iterations). If there are too many elements in the given range, this can spawn too many
     * tasks.
     *
     * Does not preserve locality, but is ensures that the tasks are filling up the worker threads
     * in the best possible way.
     */
    naive_partition,
};

/**
 * @brief      Hints to alter the behavior of a @ref conc_for algorithm
 *
 * The hints in this structure can influence the behavior of the @ref conc_for algorithm, but the
 * algorithm can decide to ignore these hints.
 *
 * In general, the algorithm performs well on a large variety of cases, so manually giving hints to
 * it is not usually needed.
 *
 * @see for_method
 */
struct for_hints {
    //! The method to be preferred when doing the concurrent for
    for_method method_ = for_method::auto_partition;
    //! The granularity of the algorithm.
    //!
    //! When choosing how many iterations to handle in one task, this parameter can instruct the
    //! algorithm to not place less than the value here. This can be used when the iterations are
    //! really small, and the task management overhead can become significant.
    //! Does not apply to the @ref for_method::upfront_partition method
    int granularity_{1};
};

} // namespace v1

namespace detail {

//! Work unit that just applies the functor to the elements, given to the partition work functions.
template <typename It, typename UnaryFunction>
struct conc_for_work {
    const UnaryFunction* ftor_{nullptr};

    conc_for_work() = default;
    conc_for_work(const conc_for_work&) = default;
    conc_for_work& operator=(const conc_for_work&) = default;

    explicit conc_for_work(const UnaryFunction& f)
        : ftor_(&f) {}

    void exec(It elem) { (*ftor_)(*elem); }
    void exec(It first, It last) {
        for (; first != last; first++)
            (*ftor_)(*first);
    }
    static constexpr bool needs_join() { return false; }
    void join(const conc_for_work&) {}
};

//! Constructs a task to implement conc_for algorithm, based on the hinted algorithm.
//! This is used if the range uses random-access iterators.
template <typename RandomIt, typename UnaryFunction>
inline task get_for_task(RandomIt first, RandomIt last, const UnaryFunction& f, task_group grp,
        for_hints hints, std::random_access_iterator_tag) {
    int granularity = std::max(1, hints.granularity_);
    switch (hints.method_) {
    case for_method::upfront_partition:
        return task(
                [first, last, &f, grp]() {
                    int n = static_cast<int>(last - first);
                    conc_for_work<RandomIt, UnaryFunction> work(f);
                    detail::upfront_partition_work(first, n, work, grp);
                },
                grp);
    case for_method::iterative_partition:
        return task(
                [first, last, &f, grp, granularity]() {
                    conc_for_work<RandomIt, UnaryFunction> work(f);
                    detail::iterative_partition_work(first, last, work, grp, granularity);
                },
                grp);
    case for_method::naive_partition:
        return task(
                [first, last, &f, grp, granularity]() {
                    conc_for_work<RandomIt, UnaryFunction> work(f);
                    detail::naive_partition_work(first, last, work, grp, granularity);
                },
                grp);
    case for_method::auto_partition:
    default:
        return task(
                [first, last, &f, grp, granularity]() {
                    int n = static_cast<int>(last - first);
                    conc_for_work<RandomIt, UnaryFunction> work(f);
                    detail::auto_partition_work(first, n, work, grp, granularity);
                },
                grp);
    }
}

//! Constructs a task to implement conc_for algorithm, based on the hinted algorithm.
//! This is used if the range DOES NOT use random-access iterators.
template <typename It, typename UnaryFunction>
inline task get_for_task(It first, It last, const UnaryFunction& f, task_group grp,
        for_hints hints, ...) {
    int granularity = std::max(1, hints.granularity_);
    switch (hints.method_) {
    case for_method::naive_partition:
        return task(
                [first, last, &f, grp, granularity]() {
                    conc_for_work<It, UnaryFunction> work(f);
                    detail::naive_partition_work(first, last, work, grp, granularity);
                },
                grp);
    case for_method::iterative_partition:
    default:
        return task(
                [first, last, &f, grp, granularity]() {
                    conc_for_work<It, UnaryFunction> work(f);
                    detail::iterative_partition_work(first, last, work, grp, granularity);
                },
                grp);
    }
}

//! Main implementation of the conc_for algorithm
template <typename Iter, typename UnaryFunction>
inline void conc_for(
        Iter first, Iter last, const UnaryFunction& f, task_group grp, for_hints hints) {
    auto& tsys = detail::get_task_system();
    auto worker_data = tsys.enter_worker();

    if (!grp)
        grp = task_group::current_task_group();
    auto wait_grp = task_group::create(grp);
    auto t = get_for_task(first, last, f, wait_grp, hints,
            typename std::iterator_traits<Iter>::iterator_category());
    tsys.spawn(std::move(t), false);
    tsys.busy_wait_on(wait_grp);

    tsys.exit_worker(worker_data);
}

} // namespace detail

inline namespace v1 {

/**
 * @brief      A concurrent `for` algorithm.
 *
 * @param      first          Iterator pointing to the first element in a collection
 * @param      last           Iterator pointing to the last element in a collection (1 past the end)
 * @param      f              Functor to apply to each element of the collection
 * @param      grp            Group in which to execute the tasks
 * @param      hints          Hints that may be passed to the
 *
 * @tparam     It             The type of the iterator to use
 * @tparam     UnaryFunction  The type of function to be applied for each element
 *
 * If there are no dependencies between the iterations of a for loop, then those iterations can be
 * run in parallel. This function attempts to parallelize these iterations. On a machine that has
 * a very large number of cores, this can execute each iteration on a different core.
 *
 * This ensure that the given functor is called exactly once for each element from the given
 * sequence. But the call may happen on different threads.
 *
 * The function does not return until all the iterations are executed. (It may execute other
 * non-related tasks while waiting for the conc_for tasks to complete).
 *
 * This generates internal tasks by spawning and waiting for those tasks to complete. If the user
 * spawns other tasks during the execution of an iteration, those tasks would also be waited on.
 * This can be a method of generating more work in the concurrent `for` loop.
 *
 * One can cancel the execution of the tasks by passing a task_group in, and canceling that
 * task_group.
 *
 * One can also provide hints to the implementation to fine-tune the algorithms to better fit the
 * data it operates on. Please note however that the implementation may completely ignore all the
 * hints it was provided.
 *
 * @warning    If the iterations are not completely independent, this results in undefined behavior.
 *
 * @see     for_hints, for_method
 */
template <typename It, typename UnaryFunction>
inline void conc_for(It first, It last, const UnaryFunction& f, task_group grp, for_hints hints) {
    detail::conc_for(first, last, f, grp, hints);
}
//! \overload
template <typename It, typename UnaryFunction>
inline void conc_for(It first, It last, const UnaryFunction& f, task_group grp) {
    detail::conc_for(first, last, f, grp, {});
}
//! \overload
template <typename It, typename UnaryFunction>
inline void conc_for(It first, It last, const UnaryFunction& f, for_hints hints) {
    detail::conc_for(first, last, f, {}, hints);
}
//! \overload
template <typename It, typename UnaryFunction>
inline void conc_for(It first, It last, const UnaryFunction& f) {
    detail::conc_for(first, last, f, {}, {});
}

} // namespace v1
} // namespace concore
