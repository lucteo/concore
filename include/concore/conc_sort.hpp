/**
 * @file    conc_sort.hpp
 * @brief   Definition of conc_sort()
 *
 * @see     conc_sort()
 */
#pragma once

#include "concore/partition_hints.hpp"
#include "concore/task_group.hpp"
#include "concore/detail/partition_work_scan.hpp"
#include "concore/detail/except_utils.hpp"

#include <algorithm>
#include <type_traits>

namespace concore {

namespace detail {

static constexpr int size_threshold = 500;

template <typename It, typename Comp>
inline int median3(It it, int l, int m, int r, const Comp& comp) {
    return comp(it[l], it[m]) ? (comp(it[m], it[r]) ? m : (comp(it[l], it[r]) ? r : l))
                              : (comp(it[r], it[m]) ? m : (comp(it[l], it[l]) ? r : l));
}

template <typename It, typename Comp>
inline int median9(It it, int n, const Comp& comp) {
    assert(n >= 8);
    int stride = n / 8;
    int m1 = median3(it, 0, stride, stride * 2, comp);
    int m2 = median3(it, stride * 3, stride * 4, stride * 5, comp);
    int m3 = median3(it, stride * 6, stride * 7, n - 1, comp);
    return median3(it, m1, m2, m3, comp);
}

template <typename It, typename Comp>
inline int partition(It begin, int n, const Comp& comp) {
    int m = median9(begin, n, comp);
    if (m != 0)
        std::swap(begin[0], begin[m]);
    auto& pivot = begin[0];

    int i = 0;
    int j = n;
    while (true) {
        assert(i < j);
        do {
            i++;
        } while (comp(begin[i], pivot));
        do {
            j--;
        } while (comp(pivot, begin[j]));
        if (i >= j)
            break;
        std::swap(begin[i], begin[j]);
    }

    std::swap(begin[j], begin[0]);
    return j;
}

template <typename It, typename Comp>
inline void conc_quicksort(It begin, int n, const Comp& comp, task_group grp) {
    while (n > size_threshold) {
        // Partition the data; elements [0, mid) < [mid] <= [mid+1, n)
        auto mid = partition(begin, n, comp);

        // Create a task for the right-hand side
        auto rhs_begin = begin + mid + 1;
        auto rhs_n = n - mid - 1;
        auto fun_rhs = [rhs_begin, rhs_n, comp, grp] {
            conc_quicksort(rhs_begin, rhs_n, comp, grp);
        };
        spawn(task{fun_rhs, grp});

        // Recurse down, and handle lhs here
        n = mid;
    }
    // It doesn't make sense to do this concurrently anymore; run the serial sort
    std::sort(begin, begin + n, comp);
}

template <typename It, typename Comp>
inline void conc_sort(It begin, It end, const Comp& comp, task_group grp) {
    int n = static_cast<int>(end - begin);
    if (n <= size_threshold) {
        std::sort(begin, end, comp);
        return;
    }

    if (!grp)
        grp = task_group::current_task_group();
    auto wait_grp = task_group::create(grp);
    std::exception_ptr thrown_exception;
    install_except_propagation_handler(thrown_exception, wait_grp);

    // Run the quicksort algorithm
    conc_quicksort(begin, n, comp, wait_grp);

    auto& ctx = detail::get_exec_context();
    auto worker_data = detail::enter_worker(ctx);
    detail::busy_wait_on(ctx, wait_grp);
    detail::exit_worker(ctx, worker_data);

    // If we have an exception, re-throw it
    if (thrown_exception)
        std::rethrow_exception(thrown_exception);
}
} // namespace detail

/**
 * @brief Concurrently sort a collection of elements
 *
 * @param begin Iterator pointing to the first element in the collection
 * @param end   Iterator pointing to one past the last element in the collection
 * @param comp  The comparison functor
 * @param grp   Group in which to execute the tasks
 *
 * @tparam  It   The type of the iterator over the collection of elements
 * @tparam  Comp The type of the comparison functor
 *
 * @details
 *
 * Sorts the given collection of elements concurrently. The comparison function must be able to be
 * called in parallel without causing any data races.
 */
template <typename It, typename Comp>
inline void conc_sort(It begin, It end, const Comp& comp, task_group grp) {
    detail::conc_sort(begin, end, comp, grp);
}
//! @overload
template <typename It, typename Comp>
inline void conc_sort(It begin, It end, const Comp& comp) {
    detail::conc_sort(begin, end, comp, {});
}
//! @overload
template <typename It>
inline void conc_sort(It begin, It end) {
    detail::conc_sort(begin, end, std::less<typename std::iterator_traits<It>::value_type>(), {});
}
//! @overload
template <typename It>
inline void conc_sort(It begin, It end, task_group grp) {
    detail::conc_sort(begin, end, std::less<typename std::iterator_traits<It>::value_type>(), grp);
}

} // namespace concore
