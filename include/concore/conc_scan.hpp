/**
 * @file    conc_scan.hpp
 * @brief   Definition of conc_scan()
 *
 * @see     conc_scan()
 */
#pragma once

#include "concore/partition_hints.hpp"
#include "concore/task_group.hpp"
#include "concore/detail/partition_work_scan.hpp"
#include "concore/detail/except_utils.hpp"

namespace concore {

namespace detail {

template <typename It, typename It2, typename Value, typename BinaryOp>
struct conc_scan_work {
    It first_;
    It2 d_first_;
    Value sum_;
    const BinaryOp* func_{nullptr};
    int line_{-1};

    conc_scan_work() = default;
    ~conc_scan_work() = default;
    conc_scan_work(const conc_scan_work&) = default;
    conc_scan_work& operator=(const conc_scan_work&) = default;
    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    conc_scan_work(conc_scan_work&&) = default;
    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    conc_scan_work& operator=(conc_scan_work&&) = default;

    conc_scan_work(It first, It2 d_first, Value id, const BinaryOp& func)
        : first_(std::move(first))
        , d_first_(std::move(d_first))
        , sum_(std::move(id))
        , func_(&func) {}

    void exec(It first, It last, work_stage stage) {
        Value cur_val = sum_;
        // auto first_save = first;
        if (stage == work_stage::initial) {
            for (; first != last; first++)
                cur_val = (*func_)(std::move(cur_val), *first);
            sum_ = std::move(cur_val);
        } else {
            auto diff = first - first_;
            for (; first != last; first++, diff++) {
                cur_val = (*func_)(std::move(cur_val), *first);
                *(d_first_ + diff) = cur_val;
            }
            if (stage == work_stage::both)
                sum_ = std::move(cur_val);
        }
    }

    void join(conc_scan_work& rhs) { rhs.sum_ = (*func_)(sum_, std::move(rhs.sum_)); }
};

template <typename It, typename It2, typename Value, typename BinaryOp>
inline Value linear_scan(It first, It last, It2 d_first, Value init, BinaryOp op) {
    for (; first != last; ++first, ++d_first) {
        init = op(init, *first);
        *d_first = init;
    }
    return init;
}

//! Main implementation of the conc_scan algorithm
template <typename It, typename It2, typename Value, typename BinaryOp>
inline Value conc_scan_it(It first, It last, It2 d_first, Value identity, const BinaryOp& op,
        task_group grp, partition_hints hints, std::random_access_iterator_tag) {
    auto& ctx = detail::get_exec_context();

    // Check if it's worth doing it in parallel
    // As the parallel algorithm creates twice as much total work, we need to ensure that we have
    // enough elements to sum
    int granularity = std::max(1, hints.granularity_);
    int n = static_cast<int>(last - first);
    if (n / granularity <= detail::num_worker_threads(ctx) * 2)
        return linear_scan(first, last, d_first, identity, op);

    auto worker_data = detail::enter_worker(ctx);

    if (!grp)
        grp = task_group::current_task_group();
    auto ex_grp = task_group::create(grp);
    std::exception_ptr thrown_exception;
    install_except_propagation_handler(thrown_exception, ex_grp);

    // Get the task to be run
    Value res;
    conc_scan_work<It, It2, Value, BinaryOp> work(first, d_first, std::move(identity), op);
    detail::auto_partition_work_scan(first, n, work, ex_grp, granularity);
    res = std::move(work.sum_);

    detail::exit_worker(ctx, worker_data);

    // If we have an exception, re-throw it
    if (thrown_exception)
        std::rethrow_exception(thrown_exception);

    return res;
}

//! Fallback
template <typename It, typename It2, typename Value, typename BinaryOp>
inline Value conc_scan_it(It first, It last, It2 d_first, Value identity, const BinaryOp& op,
        task_group, partition_hints hints, ...) {
    return linear_scan(first, last, d_first, identity, op);
}

//! Dispatch on iterator category.
template <typename It, typename It2, typename Value, typename BinaryOp>
inline Value conc_scan(It first, It last, It2 d_first, Value identity, const BinaryOp& op,
        task_group grp, partition_hints hints) {
    auto iter_cat = typename std::iterator_traits<It>::iterator_category();
    return conc_scan_it(first, last, d_first, identity, op, grp, hints, iter_cat);
}

} // namespace detail

inline namespace v1 {

/**
 * @brief      A concurrent scan algorithm
 *
 * @param      first     Iterator pointing to the first element in the collection
 * @param      last      Iterator pointing to the last element in the collection (1 past the end)
 * @param      d_first   Iterator pointing to the first element in the destination collection
 * @param      identity  The identity element (i.e., 0)
 * @param      op        The operation to be applied (i.e., summation)
 * @param      grp       Group in which to execute the tasks
 * @param      hints     Hints that may be passed to the algorithm
 *
 * @tparam     It        The type of the iterator in the input collection
 * @tparam     It2       The type of the output iterator
 * @tparam     Value     The type of the values we are operating on
 * @tparam     BinaryOp  The type of the binary operation (i.e., summation)
 *
 * @return     The result value after applying the operation to the input collection
 *
 * @details
 *
 * This implements the prefix sum algorithm. Assuming the given operation is summation, this will
 * write in the destination corresponding to each element, the sum of the previous elements,
 * including itself. Similar to std::inclusive_sum.
 *
 * This will try to parallelize the prefix sum algorithm. It will try to create enough task to make
 * all the sums in parallel. In the process of parallelizing, this will create twice as much work as
 * the serial algorithm
 *
 * One can also provide hints to the implementation to fine-tune the algorithms to better fit the
 * data it operates on. Please note however that the implementation may completely ignore all the
 * hints it was provided.
 *
 * The operation needs to be able to be called in parallel.
 *
 * @see     conc_for(), conc_reduce(), task_group
 */
template <typename It, typename It2, typename Value, typename BinaryOp>
inline Value conc_scan(It first, It last, It2 d_first, Value identity, const BinaryOp& op,
        task_group grp, partition_hints hints) {
    return detail::conc_scan(first, last, d_first, identity, op, grp, hints);
}
//! \overload
template <typename It, typename It2, typename Value, typename BinaryOp>
inline Value conc_scan(
        It first, It last, It2 d_first, Value identity, const BinaryOp& op, task_group grp) {
    return detail::conc_scan(first, last, d_first, identity, op, grp, {});
}
//! \overload
template <typename It, typename It2, typename Value, typename BinaryOp>
inline Value conc_scan(
        It first, It last, It2 d_first, Value identity, const BinaryOp& op, partition_hints hints) {
    return detail::conc_scan(first, last, d_first, identity, op, {}, hints);
}
//! \overload
template <typename It, typename It2, typename Value, typename BinaryOp>
inline Value conc_scan(It first, It last, It2 d_first, Value identity, const BinaryOp& op) {
    return detail::conc_scan(first, last, d_first, identity, op, {}, {});
}

} // namespace v1
} // namespace concore
