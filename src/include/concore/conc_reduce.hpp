/**
 * @file    conc_reduce.hpp
 * @brief   Definition of conc_reduce()
 *
 * @see     conc_reduce()
 */
#pragma once

#include "concore/detail/partition_work.hpp"
#include "concore/detail/except_utils.hpp"

namespace concore {

namespace detail {

template <typename It, typename Value, typename BinaryOp, typename ReductionOp>
struct conc_reduce_work {
    using iterator = It;

    Value value_;
    const BinaryOp* func_{nullptr};
    const ReductionOp* reduction_{nullptr};

    conc_reduce_work() = default;
    ~conc_reduce_work() = default;
    conc_reduce_work(const conc_reduce_work&) = default;
    conc_reduce_work& operator=(const conc_reduce_work&) = default;
    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    conc_reduce_work(conc_reduce_work&&) = default;
    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    conc_reduce_work& operator=(conc_reduce_work&&) = default;

    conc_reduce_work(Value id, const BinaryOp& func, const ReductionOp& reduction)
        : value_(std::move(id))
        , func_(&func)
        , reduction_(&reduction) {}

    void exec(It first, It last) {
        for (; first != last; first++)
            value_ = (*func_)(std::move(value_), safe_dereference(first, nullptr));
    }
    void join(conc_reduce_work& rhs) {
        value_ = (*reduction_)(std::move(value_), std::move(rhs.value_));
    }
};

// Case where we have random-access iterators
template <typename WorkType>
inline void do_conc_reduce(typename WorkType::iterator first, typename WorkType::iterator last,
        WorkType& work, task_group& grp, partition_hints hints, std::random_access_iterator_tag) {

    int n = static_cast<int>(last - first);
    if (n == 0)
        return;
    int granularity = compute_granularity(n, hints);
    switch (hints.method_) {
    case partition_method::upfront_partition: {
        int tasks_per_worker = hints.tasks_per_worker_ > 0 ? hints.tasks_per_worker_ : 2;
        detail::upfront_partition_work<true>(first, n, work, grp, tasks_per_worker);
        break;
    }
    case partition_method::naive_partition: // naive cannot be efficiently implemented for reduce
    case partition_method::iterative_partition:
        detail::iterative_partition_work<true>(first, last, work, grp, granularity);
        break;
    case partition_method::auto_partition:
    default:
        detail::auto_partition_work<true>(first, n, work, grp, granularity);
        break;
    }
}
// Integral case: behave as we have random-access iterators
template <typename WorkType>
inline void do_conc_reduce(typename WorkType::iterator first, typename WorkType::iterator last,
        WorkType& work, task_group& grp, partition_hints hints, no_iterator_tag) {
    do_conc_reduce(first, last, work, grp, hints, std::random_access_iterator_tag());
}
// Forward iterators case
template <typename WorkType>
inline void do_conc_reduce(typename WorkType::iterator first, typename WorkType::iterator last,
        WorkType& work, task_group& grp, partition_hints hints, ...) {
    int granularity = std::max(1, hints.granularity_);
    detail::iterative_partition_work<true>(first, last, work, grp, granularity);
}

template <typename WorkType>
inline void conc_reduce_impl(typename WorkType::iterator first, typename WorkType::iterator last,
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
        do_conc_reduce(first, last, work, wait_grp, hints, it_cat);
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

template <typename It, typename Value, typename BinaryOp, typename ReductionOp>
inline Value conc_reduce_fun(It first, It last, Value identity, const BinaryOp& op,
        const ReductionOp& reduction, task_group grp, partition_hints hints) {
    detail::conc_reduce_work<It, Value, BinaryOp, ReductionOp> work(identity, op, reduction);
    conc_reduce_impl(first, last, work, grp, hints);
    return work.value_;
}
} // namespace detail

inline namespace v1 {

/**
 * @brief      A concurrent reduce algorithm.
 *
 * @param      first          Iterator pointing to the first element in a collection
 * @param      last           Iterator pointing to the last element in a collection (1 past the end)
 * @param      identity       The identity value; the starting point of the reduce
 * @param      op             Binary functor that is applied to the elements
 * @param      reduction      Binary functor that reduces the values
 * @param      grp            Group in which to execute the tasks
 * @param      hints          Hints that may be passed to the
 *
 * @param      work           The work to be applied to be executed for the elements (when not using
 *                            `f`)
 *
 * @tparam     It             The type of the iterator to use
 * @tparam     Value          The type of the value used for computation
 * @tparam     BinaryOp       The type of function used by the operation that applies to elements
 * @tparam     ReductionOp    The type of function used by the reduction operation
 *
 * @tparam     WorkType       The type of a work object to be used (when not using `op` and
 *                            `reduction`)
 *
 * @details
 *
 * This implements a reduce operation (similar to `std::accumulate`) but does it by exposing
 * concurrent computations. In order for this to be well defined, the given operation and reduction
 * functors must be able to properly run in parallel on different parts of the input collection.
 *
 * The operation is called exactly once for each element. The reduction is called each time we need
 * to combine the results of two computations.
 *
 * This generates internal tasks by spawning and waiting for those tasks to complete. If the user
 * spawns other tasks during the execution of an iteration, those tasks would also be waited on.
 *
 * One can cancel the execution of the tasks by passing a task_group object in, and canceling that
 * task_group.
 *
 * One can also provide hints to the implementation to fine-tune the algorithms to better fit the
 * data it operates on. Please note however that the implementation may completely ignore all the
 * hints it was provided.
 *
 * @warning    If the iterations are not completely independent, this results in undefined behavior.
 *
 * @see     partition_hints, partition_method, conc_for(), task_group
 */
template <typename It, typename Value, typename BinaryOp, typename ReductionOp>
inline Value conc_reduce(It first, It last, Value identity, const BinaryOp& op,
        const ReductionOp& reduction, task_group grp, partition_hints hints) {
    return detail::conc_reduce_fun(first, last, identity, op, reduction, grp, hints);
}
//! \overload
template <typename It, typename Value, typename BinaryOp, typename ReductionOp>
inline Value conc_reduce(It first, It last, Value identity, const BinaryOp& op,
        const ReductionOp& reduction, task_group grp) {
    return detail::conc_reduce_fun(first, last, identity, op, reduction, grp, {});
}
//! \overload
template <typename It, typename Value, typename BinaryOp, typename ReductionOp>
inline Value conc_reduce(It first, It last, Value identity, const BinaryOp& op,
        const ReductionOp& reduction, partition_hints hints) {
    return detail::conc_reduce_fun(first, last, identity, op, reduction, {}, hints);
}
//! \overload
template <typename It, typename Value, typename BinaryOp, typename ReductionOp>
inline Value conc_reduce(
        It first, It last, Value identity, const BinaryOp& op, const ReductionOp& reduction) {
    return detail::conc_reduce_fun(first, last, identity, op, reduction, {}, {});
}

//! \overload
template <typename WorkType>
inline void conc_reduce(typename WorkType::iterator first, typename WorkType::iterator last,
        WorkType& work, const task_group& grp, partition_hints hints) {
    detail::conc_reduce_impl(first, last, work, grp, hints);
}
//! \overload
template <typename WorkType>
inline void conc_reduce(typename WorkType::iterator first, typename WorkType::iterator last,
        WorkType& work, const task_group& grp) {
    detail::conc_reduce_impl(first, last, work, grp, {});
}
//! \overload
template <typename WorkType>
inline void conc_reduce(typename WorkType::iterator first, typename WorkType::iterator last,
        WorkType& work, partition_hints hints) {
    detail::conc_reduce_impl(first, last, work, {}, hints);
}
//! \overload
template <typename WorkType>
inline void conc_reduce(
        typename WorkType::iterator first, typename WorkType::iterator last, WorkType& work) {
    detail::conc_reduce_impl(first, last, work, {}, {});
}

} // namespace v1
} // namespace concore
