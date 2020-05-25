#pragma once

#include "concore/task_group.hpp"
#include "concore/spawn.hpp"

namespace concore {

inline namespace v1 {

/**
 * @brief      Hint indicating the method of dividing the work within a conc_for
 *
 * Using this would provide a hint the conc_for algorithm on how to partition the input data.
 *
 * The implementation may not follow the given advice.
 */
enum class for_method {
    auto_partition, //!< Automatically partitions the data, trying to maximize locality, and trying
                    //!< to keep the workers busy at the right amount.
    upfront_partition, //!< Partitions the data upfront, assuming an equal distribution of work.
    naive_partition,   //!< A naive partition that assumes one task per item.
};

struct for_hints {
    //! The method to be preferred when doing the concurrent for
    for_method method_ = for_method::auto_partition;
    //! The granularity of the algorithm; if possible no tasks with less than this number of
    //! elements will be taken.
    //! Does not apply to the @ref for_method::upfront_partition method
    int granularity_{1};
};

} // namespace v1

namespace detail {

template <typename It, typename UnaryFunction>
inline void run_serially(It first, It last, const UnaryFunction& f) {
    for (; first != last; first++)
        f(*first);
}

/**
 * @brief      The auto-partition implementation of the conc_for algorithm.
 *
 * @param      first          Iterator pointing to the first element
 * @param      n              The number of elements
 * @param      f              The functor to be applied to each element
 * @param      grp            The group we put our tasks in
 * @param      granularity    The granularity we use in the implementation
 *
 * @tparam     RandomIt       The type of random iterator we use for our range of elements
 * @tparam     UnaryFunction  The type of function to be applied for each element
 *
 * This will recursively divide the given range in half; spawn a task for handling the right half
 * and deal with the left half directly. In the absence of worker threads, the right hand tasks will
 * be executed at a much later point. In this context, to maximize locality, the left-hand tasks may
 * serve elements from the right side.
 *
 * The general algorithm is along the following lines:
 *  - divide the range in half; spawn a task for the right side, handle left side locally
 *  - use an atomic variable between the two sides, to properly divide the tasks
 *  - if the right-hand task does not start early enough, the left-hand task may steal work from it
 *  by changing the value of the atomic variable
 *  - on the left-hand side we divided and spawn new tasks all the way down
 *  - on the right-hand side we recursively apply this algorithm to fix remaining elements
 *
 * If no granularity is given, the negotiation between left and right is done at the element level.
 * Specifying a greater granularity will make tasks considered in bulk. For example, if granularity
 * is 10, we always execute chunk of 10 elements.
 *
 * This is the default partitioning algorithm. It works well if the work is not well-balanced
 * between different values in the given range.
 *
 * It only works for random-access iterators.
 */
template <typename RandomIt, typename UnaryFunction>
inline void for_auto_partition(
        RandomIt first, int n, const UnaryFunction& f, task_group grp, int granularity) {
    if (n <= granularity) {
        // Cannot split anymore; just execute work
        run_serially(first, first + std::min(n, granularity), f);
        return;
    }

    // We assume we can't have more than 2^32 elements, so we can have max 32 half-way splits
    static constexpr int max_num_splits = 32;
    // Preallocate a stack of mid points
    int mid_values[max_num_splits];
    std::atomic<int> mid_atomic_values[max_num_splits];

    auto wait_grp = task_group::create(grp);

    // Iterate down, at each step splitting the range into half; stop when we reached one element
    int level = 0;
    int end = n;
    while (end > granularity) {
        // Current interval: [*first, *first+end)

        // Spawn a task for the second half our our current interval
        int cur_mid = (end + 1) / 2;
        mid_values[level] = cur_mid;
        mid_atomic_values[level].store(cur_mid, std::memory_order_relaxed);
        std::atomic<int>& mid = mid_atomic_values[level];
        auto fun_second_half = [first, end, &mid, &f, grp, granularity]() {
            // Pick up where the first part ended
            int cur_mid = mid.load(std::memory_order_acquire);
            // int cur_mid = mid.load(std::memory_order_relaxed);
            while (cur_mid < end) {
                if (mid.compare_exchange_weak(cur_mid, -1, std::memory_order_acq_rel))
                    break;
            }
            // Divide what's left of the interval
            if (cur_mid < end)
                for_auto_partition(first + cur_mid, end - cur_mid, f, grp, granularity);
        };
        spawn(task{fun_second_half, wait_grp});

        // Now, handle in the current task the first half of the range
        level++;
        end = cur_mid;
    }
    level--;

    // Now, left-to-right start executing the given functor
    // At each step, update the corresponding atomic to make sure other tasks are not executing the
    // same tasks If right-hand tasks did not start, steal iterations from them
    int our_max = mid_values[level];
    int i = 0;
    while (i < n) {
        // Run as many iterations as we can
        run_serially(first + i, first + our_max, f);
        i = our_max;
        if (our_max >= n)
            break;
        // If we can, try to steal some more from the right-hand task
        int lvl_end = level == 0 ? n : mid_values[level - 1];
        int steal_end = std::min(our_max + granularity, lvl_end);
        int old_val = our_max;
        if (!mid_atomic_values[level].compare_exchange_strong(
                    old_val, steal_end, std::memory_order_acq_rel))
            break;
        our_max = steal_end;
        // If we consumed everything from the right-side, move one level up
        if (our_max >= lvl_end)
            level--;
    }

    // Wait for all the spawned tasks to be completed
    wait(wait_grp);
}

/**
 * @brief      The upfront-partition implementation of the conc_for algorithm
 *
 * @param      first          Iterator pointing to the first element
 * @param      n              The number of elements
 * @param      f              The functor to be applied to each element
 * @param      grp            The group we put our tasks in
 *
 * @tparam     RandomIt       The type of random iterator we use for our range of elements
 * @tparam     UnaryFunction  The type of function to be applied for each element
 *
 * This strategy tries to do an upfront division of tasks. If we have N workers threads, this will
 * try to create 2*N tasks for the given range, splitting the tasks accordingly.
 *
 * If there are not enough elements in the range, it will create a task for each element.
 *
 * This strategy doesn't work well if the work is not evenly balanced between different elements.
 * Also, if the different tasks take a long time, some longer than others, it will not subdivide
 * remaining tasks to ensure that all the workers have work to do.
 *
 * It only works for random-access iterators.
 */
template <typename RandomIt, typename UnaryFunction>
inline void for_upfront_partition(RandomIt first, int n, const UnaryFunction& f, task_group grp) {
    auto wait_grp = task_group::create(grp);

    auto& tsys = detail::get_task_system();
    auto c = tsys.num_worker_threads();

    int num_tasks = c * 2;
    if (num_tasks < n) {
        for (int i = 0; i < num_tasks; i++) {
            auto start = first + (n * i / num_tasks);
            auto end = first + (n * (i + 1) / num_tasks);
            spawn(task{[f, start, end]() { run_serially(start, end, f); }, wait_grp});
        }
    } else {
        for (int i = 0; i < n; i++) {
            spawn(task{[f, first, i]() { f(*(first + i)); }, wait_grp});
        }
    }

    // Wait for all the spawned tasks to be completed
    wait(wait_grp);
}

/**
 * @brief      The naive-partition implementation of the conc_for algorithm
 *
 * @param      first          Iterator pointing to the first element in the range
 * @param      last           Iterator pointing to the last element in the range
 * @param      f              { parameter_description }
 * @param      f              The functor to be applied to each element
 * @param      grp            The group we put our tasks in
 * @param      granularity    The granularity we use in the implementation
 *
 * @tparam     It             The type of the iterator to use
 * @tparam     UnaryFunction  The type of function to be applied for each element
 *
 * This partitioning is very simple: it creates a task for each element. If granularity greater than
 * 1 is given, it will create a task for multiple elements.
 *
 * This works relatively well if we have a relatively small number of elements, where we can't
 * subdivide the range too much. Instead of bothering with work-stealing between partitions, we just
 * create tasks for every element.
 *
 * This method also works for forward iterators, not just for random-access iterators.
 */
template <typename It, typename UnaryFunction>
inline void for_naive_partition(
        It first, It last, const UnaryFunction& f, task_group grp, int granularity) {
    auto wait_grp = task_group::create(grp);

    if (granularity <= 1) {
        for (; first != last; first++) {
            spawn(task{[f, first]() { f(*first); }, wait_grp});
        }
    } else {
        auto it = first;
        auto ite = it;
        while (ite != last) {
            // find the end of the stride
            for (int i = 0; i < granularity && ite != last; i++) {
                ite++;
            }
            // spawn a task to cover the current stride
            spawn(task{[f, it, ite]() { run_serially(it, ite, f); }, wait_grp});

            it = ite;
        }
    }
    // Wait for all the spawned tasks to be completed
    wait(wait_grp);
}

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
                    detail::for_upfront_partition(first, n, f, grp);
                },
                grp);
    case for_method::naive_partition:
        return task(
                [first, last, &f, grp, granularity]() {
                    detail::for_naive_partition(first, last, f, grp, granularity);
                },
                grp);
    case for_method::auto_partition:
    default:
        return task(
                [first, last, &f, grp, granularity]() {
                    int n = static_cast<int>(last - first);
                    detail::for_auto_partition(first, n, f, grp, granularity);
                },
                grp);
    }
}

//! Constructs a task to implement conc_for algorithm, based on the hinted algorithm.
//! This is used if the range DOES NOT use random-access iterators.
template <typename RandomIt, typename UnaryFunction>
inline task get_for_task(RandomIt first, RandomIt last, const UnaryFunction& f, task_group grp,
        for_hints hints, ...) {
    int granularity = std::max(1, hints.granularity_);
    return task(
            [first, last, &f, grp, granularity]() {
                detail::for_naive_partition(first, last, f, grp, granularity);
            },
            grp);
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
 * @brief      A concurrent for algorithm
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
 * If the iterations are not completely independent, this results in undefined behavior.
 *
 * One can cancel the execution of the tasks by passing a task_group in, and canceling that
 * task_group.
 *
 * One can also provide hints to the implementation to fine-tune the algorithms to better fit the
 * data it operates on. Please note however that the implementation may completely ignore all the
 * hints it was provided.
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
