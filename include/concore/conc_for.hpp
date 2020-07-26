#pragma once

#include "concore/partition_hints.hpp"
#include "concore/task_group.hpp"
#include "concore/spawn.hpp"
#include "concore/detail/partition_work.hpp"
#include "concore/detail/algo_utils.hpp"
#include "concore/detail/except_utils.hpp"

namespace concore {

namespace detail {

//! Work unit that just applies the functor to the elements, given to the partition work functions.
template <typename It, typename UnaryFunction>
struct conc_for_work {
    const UnaryFunction* ftor_{nullptr};

    using iterator = It;

    conc_for_work() = default;
    conc_for_work(const conc_for_work&) = default;
    conc_for_work& operator=(const conc_for_work&) = default;

    explicit conc_for_work(const UnaryFunction& f)
        : ftor_(&f) {}

    void exec(It first, It last) {
        for (; first != last; first++)
            (*ftor_)(safe_dereference(first, nullptr));
    }
};

// Case where we have random-access iterators
template <typename WorkType>
inline void do_conc_for(typename WorkType::iterator first, typename WorkType::iterator last,
        WorkType& work, const task_group& grp, partition_hints hints,
        std::random_access_iterator_tag) {

    int n = static_cast<int>(last - first);
    if (n == 0)
        return;
    int granularity = compute_granularity(n, hints);
    switch (hints.method_) {
    case partition_method::upfront_partition: {
        int tasks_per_worker = hints.tasks_per_worker_ > 0 ? hints.tasks_per_worker_ : 2;
        detail::upfront_partition_work<false>(first, n, work, grp, tasks_per_worker);
        break;
    }
    case partition_method::iterative_partition:
        detail::iterative_partition_work<false>(first, last, work, grp, granularity);
        break;
    case partition_method::naive_partition:
        detail::naive_partition_work(first, last, work, grp, granularity);
        break;
    case partition_method::auto_partition:
    default:
        detail::auto_partition_work2(first, n, work, granularity);
        break;
    }
}
// Integral case: behave as we have random-access iterators
template <typename WorkType>
inline void do_conc_for(typename WorkType::iterator first, typename WorkType::iterator last,
        WorkType& work, const task_group& grp, partition_hints hints, no_iterator_tag) {
    do_conc_for(first, last, work, grp, hints, std::random_access_iterator_tag());
}
// Forward iterators case
template <typename WorkType>
inline void do_conc_for(typename WorkType::iterator first, typename WorkType::iterator last,
        WorkType& work, const task_group& grp, partition_hints hints, ...) {
    int granularity = std::max(1, hints.granularity_);
    if (hints.method_ == partition_method::naive_partition) {
        detail::naive_partition_work(first, last, work, grp, granularity);
    } else {
        detail::iterative_partition_work<false>(first, last, work, grp, granularity);
    }
}

template <typename WorkType>
inline void conc_for_impl(typename WorkType::iterator first, typename WorkType::iterator last,
        WorkType& work, const task_group& grp, partition_hints hints) {

    auto wait_grp = task_group::create(grp ? grp : task_group::current_task_group());
    std::exception_ptr thrown_exception;
    install_except_propagation_handler(thrown_exception, wait_grp);

    // Make sure that all the spawned tasks will have this group
    auto old_grp = task_group::set_current_task_group(wait_grp);

    // Start the work with the appropriate method, and wait for it to finish
    try {
        auto it_cat =
                typename detail::iterator_type<typename WorkType::iterator>::iterator_category();
        do_conc_for(first, last, work, wait_grp, hints, it_cat);
    } catch (...) {
        detail::task_group_access::on_task_exception(wait_grp, std::current_exception());
    }
    wait(wait_grp);

    // Restore the old task group
    task_group::set_current_task_group(old_grp);

    // If we have an exception, re-throw it
    if (thrown_exception)
        std::rethrow_exception(thrown_exception);
}

template <typename It, typename UnaryFunction>
inline void conc_for_fun(
        It first, It last, const UnaryFunction& f, const task_group& grp, partition_hints hints) {
    detail::conc_for_work<It, UnaryFunction> work(f);
    conc_for_impl(first, last, work, grp, hints);
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
 * @param      work           The work to be applied to be executed for the elements
 *
 * @tparam     It             The type of the iterator to use
 * @tparam     UnaryFunction  The type of function to be applied for each element
 *
 * @tparam     WorkType       The type of a work object to be used
 *
 * If there are no dependencies between the iterations of a for loop, then those iterations can be
 * run in parallel. This function attempts to parallelize these iterations. On a machine that has
 * a very large number of cores, this can execute each iteration on a different core.
 *
 * This ensure that the given work/functor is called exactly once for each element from the given
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
 * There are two forms of this function: one that uses a functor, and one that takes a
 * work as parameter. The version with the 'work' given as argument may be faster in certain cases
 * in which, between iterators, we can store temporary data.
 *
 * The work structure given to the function must have the following structure:
 *      struct GenericWorkType {
 *          using iterator = my_iterator_type;
 *          void exec(my_iterator_type first, my_iterator_type last) { ... }
 *      };
 *
 * This work will be called for various chunks from the input. The 'iterator' type defined in the
 * given work must support basic random-iterator operations, but without dereference. That is:
 * difference, incrementing, and addition with an integer. The work objects must be copyable.
 *
 * In the case that no work is given, the algorithm expects either input iterators, or integral
 * types.
 *
 * @warning    If the iterations are not completely independent, this results in undefined behavior.
 *
 * @see     partition_hints, partition_method
 */
template <typename It, typename UnaryFunction>
inline void conc_for(
        It first, It last, const UnaryFunction& f, const task_group& grp, partition_hints hints) {
    detail::conc_for_fun(first, last, f, grp, hints);
}
//! \overload
template <typename It, typename UnaryFunction>
inline void conc_for(It first, It last, const UnaryFunction& f, const task_group& grp) {
    detail::conc_for_fun(first, last, f, grp, {});
}
//! \overload
template <typename It, typename UnaryFunction>
inline void conc_for(It first, It last, const UnaryFunction& f, partition_hints hints) {
    detail::conc_for_fun(first, last, f, {}, hints);
}
//! \overload
template <typename It, typename UnaryFunction>
inline void conc_for(It first, It last, const UnaryFunction& f) {
    detail::conc_for_fun(first, last, f, {}, {});
}

//! \overload
template <typename WorkType>
inline void conc_for(typename WorkType::iterator first, typename WorkType::iterator last,
        WorkType& work, const task_group& grp, partition_hints hints) {
    detail::conc_for_impl(first, last, work, grp, hints);
}
//! \overload
template <typename WorkType>
inline void conc_for(typename WorkType::iterator first, typename WorkType::iterator last,
        WorkType& work, const task_group& grp) {
    detail::conc_for_impl(first, last, work, grp, {});
}
//! \overload
template <typename WorkType>
inline void conc_for(typename WorkType::iterator first, typename WorkType::iterator last,
        WorkType& work, partition_hints hints) {
    detail::conc_for_impl(first, last, work, {}, hints);
}
//! \overload
template <typename WorkType>
inline void conc_for(
        typename WorkType::iterator first, typename WorkType::iterator last, WorkType& work) {
    detail::conc_for_impl(first, last, work, {}, {});
}

} // namespace v1
} // namespace concore
