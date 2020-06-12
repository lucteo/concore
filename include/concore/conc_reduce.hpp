#pragma once

#include "concore/partition_hints.hpp"
#include "concore/task_group.hpp"
#include "concore/spawn.hpp"
#include "concore/detail/partition_work.hpp"
#include "concore/detail/except_utils.hpp"

namespace concore {

namespace detail {

template <typename It, typename Value, typename BinaryOp, typename ReductionFunc>
struct conc_reduce_work {
    Value value_;
    const BinaryOp* func_{nullptr};
    const ReductionFunc* reduction_{nullptr};

    conc_reduce_work() = default;
    conc_reduce_work(const conc_reduce_work&) = default;
    conc_reduce_work& operator=(const conc_reduce_work&) = default;

    conc_reduce_work(Value id, const BinaryOp& func, const ReductionFunc& reduction)
        : value_(id)
        , func_(&func)
        , reduction_(&reduction) {}

    void exec(It elem) { value_ = (*func_)(std::move(value_), *elem); }
    void exec(It first, It last) {
        for (; first != last; first++)
            value_ = (*func_)(std::move(value_), *first);
    }
    static constexpr bool needs_join() { return true; }
    void join(conc_reduce_work& rhs) {
        value_ = (*reduction_)(std::move(value_), std::move(rhs.value_));
    }
};

//! Constructs a task to implement conc_reduce algorithm, based on the hinted algorithm.
//! This is used if the range uses random-access iterators.
template <typename It, typename Value, typename BinaryOp, typename ReductionFunc>
inline task get_reduce_task(Value& res, It first, It last, Value identity, const BinaryOp& op,
        const ReductionFunc& reduction, task_group grp, partition_hints hints,
        std::random_access_iterator_tag) {
    int granularity = std::max(1, hints.granularity_);
    switch (hints.method_) {
    case partition_method::upfront_partition:
        return task(
                [&res, first, last, identity, &op, &reduction, grp]() {
                    int n = static_cast<int>(last - first);
                    conc_reduce_work<It, Value, BinaryOp, ReductionFunc> work(
                            std::move(identity), op, reduction);
                    detail::upfront_partition_work(first, n, work, grp);
                    res = std::move(work.value_);
                },
                grp);
    case partition_method::naive_partition: // naive cannot be efficiently implemented for reduce
    case partition_method::iterative_partition:
        return task(
                [&res, first, last, identity, &op, &reduction, grp, granularity]() {
                    conc_reduce_work<It, Value, BinaryOp, ReductionFunc> work(
                            std::move(identity), op, reduction);
                    detail::iterative_partition_work(first, last, work, grp, granularity);
                    res = std::move(work.value_);
                },
                grp);
    case partition_method::auto_partition:
    default:
        return task(
                [&res, first, last, identity, &op, &reduction, grp, granularity]() {
                    int n = static_cast<int>(last - first);
                    conc_reduce_work<It, Value, BinaryOp, ReductionFunc> work(
                            std::move(identity), op, reduction);
                    detail::auto_partition_work(first, n, work, grp, granularity);
                    res = std::move(work.value_);
                },
                grp);
    }
}

//! Constructs a task to implement conc_reduce algorithm, based on the hinted algorithm.
//! This is used if the range DOES NOT use random-access iterators.
template <typename It, typename Value, typename BinaryOp, typename ReductionFunc>
inline task get_reduce_task(Value& res, It first, It last, Value identity, const BinaryOp& op,
        const ReductionFunc& reduction, task_group grp, partition_hints hints, ...) {
    int granularity = std::max(1, hints.granularity_);
    switch (hints.method_) {
    case partition_method::naive_partition: // naive cannot be efficiently implemented for reduce
    case partition_method::iterative_partition:
    default:
        return task(
                [&res, first, last, identity, &op, &reduction, grp, granularity]() {
                    conc_reduce_work<It, Value, BinaryOp, ReductionFunc> work(
                            std::move(identity), op, reduction);
                    detail::iterative_partition_work(first, last, work, grp, granularity);
                    res = std::move(work.value_);
                },
                grp);
    }
}

//! Main implementation of the conc_reduce algorithm
template <typename It, typename Value, typename BinaryOp, typename ReductionFunc>
inline Value conc_reduce(It first, It last, Value identity, const BinaryOp& op,
        const ReductionFunc& reduction, task_group grp, partition_hints hints) {
    auto& tsys = detail::get_task_system();
    auto worker_data = tsys.enter_worker();

    if (!grp)
        grp = task_group::current_task_group();
    auto wait_grp = task_group::create(grp);
    std::exception_ptr thrown_exception;
    install_except_propagation_handler(thrown_exception, wait_grp);

    Value res;
    auto iter_cat = typename std::iterator_traits<It>::iterator_category();
    auto t = get_reduce_task(res, first, last, identity, op, reduction, wait_grp, hints, iter_cat);
    tsys.spawn(std::move(t), false);
    tsys.busy_wait_on(wait_grp);

    tsys.exit_worker(worker_data);

    // If we have an exception, re-throw it
    if (thrown_exception)
        std::rethrow_exception(thrown_exception);

    return res;
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
template <typename It, typename Value, typename BinaryOp, typename ReductionFunc>
inline Value conc_reduce(It first, It last, Value identity, const BinaryOp& op,
        const ReductionFunc& reduction, task_group grp, partition_hints hints) {
    return detail::conc_reduce(first, last, identity, op, reduction, grp, hints);
}
//! \overload
template <typename It, typename Value, typename BinaryOp, typename ReductionFunc>
inline Value conc_reduce(It first, It last, Value identity, const BinaryOp& op,
        const ReductionFunc& reduction, task_group grp) {
    return detail::conc_reduce(first, last, identity, op, reduction, grp, {});
}
//! \overload
template <typename It, typename Value, typename BinaryOp, typename ReductionFunc>
inline Value conc_reduce(It first, It last, Value identity, const BinaryOp& op,
        const ReductionFunc& reduction, partition_hints hints) {
    return detail::conc_reduce(first, last, identity, op, reduction, {}, hints);
}
//! \overload
template <typename It, typename Value, typename BinaryOp, typename ReductionFunc>
inline Value conc_reduce(
        It first, It last, Value identity, const BinaryOp& op, const ReductionFunc& reduction) {
    return detail::conc_reduce(first, last, identity, op, reduction, {}, {});
}

} // namespace v1
} // namespace concore
