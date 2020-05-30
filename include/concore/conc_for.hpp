#pragma once

#include "concore/partition_hints.hpp"
#include "concore/task_group.hpp"
#include "concore/spawn.hpp"
#include "concore/detail/partition_work.hpp"

namespace concore {

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
    void join(conc_for_work&) {}
};

//! Constructs a task to implement conc_for algorithm, based on the hinted algorithm.
//! This is used if the range uses random-access iterators.
template <typename RandomIt, typename UnaryFunction>
inline task get_for_task(RandomIt first, RandomIt last, const UnaryFunction& f, task_group grp,
        partition_hints hints, std::random_access_iterator_tag) {
    int granularity = std::max(1, hints.granularity_);
    switch (hints.method_) {
    case partition_method::upfront_partition:
        return task(
                [first, last, &f, grp]() {
                    int n = static_cast<int>(last - first);
                    conc_for_work<RandomIt, UnaryFunction> work(f);
                    detail::upfront_partition_work(first, n, work, grp);
                },
                grp);
    case partition_method::iterative_partition:
        return task(
                [first, last, &f, grp, granularity]() {
                    conc_for_work<RandomIt, UnaryFunction> work(f);
                    detail::iterative_partition_work(first, last, work, grp, granularity);
                },
                grp);
    case partition_method::naive_partition:
        return task(
                [first, last, &f, grp, granularity]() {
                    conc_for_work<RandomIt, UnaryFunction> work(f);
                    detail::naive_partition_work(first, last, work, grp, granularity);
                },
                grp);
    case partition_method::auto_partition:
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
        partition_hints hints, ...) {
    int granularity = std::max(1, hints.granularity_);
    switch (hints.method_) {
    case partition_method::naive_partition:
        return task(
                [first, last, &f, grp, granularity]() {
                    conc_for_work<It, UnaryFunction> work(f);
                    detail::naive_partition_work(first, last, work, grp, granularity);
                },
                grp);
    case partition_method::iterative_partition:
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
        Iter first, Iter last, const UnaryFunction& f, task_group grp, partition_hints hints) {
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
 * @see     partition_hints, partition_method
 */
template <typename It, typename UnaryFunction>
inline void conc_for(It first, It last, const UnaryFunction& f, task_group grp, partition_hints hints) {
    detail::conc_for(first, last, f, grp, hints);
}
//! \overload
template <typename It, typename UnaryFunction>
inline void conc_for(It first, It last, const UnaryFunction& f, task_group grp) {
    detail::conc_for(first, last, f, grp, {});
}
//! \overload
template <typename It, typename UnaryFunction>
inline void conc_for(It first, It last, const UnaryFunction& f, partition_hints hints) {
    detail::conc_for(first, last, f, {}, hints);
}
//! \overload
template <typename It, typename UnaryFunction>
inline void conc_for(It first, It last, const UnaryFunction& f) {
    detail::conc_for(first, last, f, {}, {});
}

} // namespace v1
} // namespace concore
