#pragma once

#include "concore/task_group.hpp"
#include "concore/global_executor.hpp"
#include "concore/task_graph.hpp"

#include <vector>

namespace concore {
namespace detail {

//! The work stage.
//! Constraints:
//!     - initial phase work cannot be done in parallel with any other work
//!     - final work (but not 'both') can be done in parallel with 'join'
//!     - two joins with the same left side can be done in parallel
enum class work_stage {
    initial,
    final,
    both,
};

// Model of a work type:
//
// template <typename It>
// struct GenericWorkType {
//     void exec(It first, It last, work_stage stage);
//     void join(GenericWorkType& rhs);
// };

namespace scan_auto_impl {

//! Given number of elements, find the number of levels needed for a perfect power-of-two division.
inline int get_num_levels(int n, int granularity) {
    int res = 1;
    while (n > 2 * granularity) {
        res++;
        n /= 2;
    }
    return res;
}

//! A processing line for an interval.
template <typename WorkType>
struct line {
    //! The work to be applied; holds the sum accumulated until the end of the interval.
    //! Node, for the final pass, this work is applied for the next range of elements.
    WorkType work_;
    //! The first task created for this line, the one that starts processing
    chained_task first_task_;
    //! The last task added for the line.
    chained_task last_task_;
    //! Last task that reads from the line; used to prevent writes to the line while others are
    //! reading from it.
    chained_task read_dep_task_;
};

//! Create a task that executes the first pass on the work of a line.
template <typename RandomIt, typename WorkType>
inline void create_first_pass_task(
        line<WorkType>& line, RandomIt first, RandomIt last, bool left_most, task_group grp) {
    auto f = [&line, first, last, left_most] {
        line.work_.exec(first, last, left_most ? work_stage::both : work_stage::initial);
    };
    line.first_task_ = chained_task{task{std::move(f), grp}};
    line.last_task_ = line.first_task_;
}
//! Creates a join work between two lines.
//! The result is added to the right line.
template <typename WorkType>
inline void create_join_task(line<WorkType>& lhs, line<WorkType>& rhs, task_group grp) {
    auto f = [&lhs, &rhs] { lhs.work_.join(rhs.work_); };
    auto join = chained_task{task{std::move(f), grp}};
    add_dependency(lhs.last_task_, join);
    add_dependency(rhs.last_task_, join);
    if (rhs.read_dep_task_)
        add_dependency(rhs.read_dep_task_, join);
    lhs.read_dep_task_ = join;
    rhs.last_task_ = std::move(join);
}
//! Create a task that executes the final pass work for a line.
//! Note: the elements passed here are the ones that correspond to the next line.
template <typename RandomIt, typename WorkType>
inline void create_final_pass_task(line<WorkType>& line, RandomIt first, RandomIt last,
        task_group grp, chained_task& final_task) {
    auto f = [&line, first, last] { line.work_.exec(first, last, work_stage::final); };
    auto t = chained_task{task{std::move(f), grp}};
    add_dependency(t, final_task);
    add_dependency(line.last_task_, t);
}

//! The main algo.
//! Creates the tasks in a task graph, and executes the task graph.
template <typename RandomIt, typename WorkType>
inline void algo(RandomIt first, int n, WorkType& work, task_group grp, int granularity) {
    // If 'n' is far bigger than the the number of workers, we will make too many divisions, and
    // create more work; try to limit the number of divisions
    auto& tsys = detail::get_task_system();
    int n2 = std::min(tsys.num_worker_threads() * 2, n);
    int num_tasks = 0;

    // Determine the number of divisions we need -- power of 2
    int num_levels = get_num_levels(n2, granularity);
    int num_div = 1 << num_levels;

    // The lines of work; last line is not used
    std::vector<line<WorkType>> lines;
    lines.resize(num_div - 1, line<WorkType>{work, {}});

    // Create the initial pass work tasks
    for (int i = 0; i < num_div - 1; i++) {
        auto start = first + (n * i / num_div);
        auto end = first + (n * (i + 1) / num_div);
        lines[i].work_.line_ = i;
        create_first_pass_task(lines[i], start, end, i == 0, grp);
        num_tasks++;
    }
    // Create the first-pass join tasks
    for (int lvl = 0; lvl < num_levels; lvl++) {
        int stride = 1 << (lvl + 1);
        for (int i = stride - 1; i < num_div - 1; i += stride) {
            create_join_task(lines[i - stride / 2], lines[i], grp);
            num_tasks++;
        }
    }
    // Create the second-pass join tasks
    for (int lvl = num_levels - 2; lvl >= 0; lvl--) {
        int stride = 1 << (lvl + 1);
        for (int i = stride - 1; i < num_div - stride / 2; i += stride) {
            create_join_task(lines[i], lines[i + stride / 2], grp);
            num_tasks++;
        }
    }

    // The task to wait on
    auto wait_grp = task_group::create(grp);
    chained_task wait_task = chained_task{task([] {}, wait_grp)};

    // Create the final pass work tasks
    for (int i = 1; i < num_div; i++) {
        auto start = first + (n * i / num_div);
        auto end = first + (n * (i + 1) / num_div);
        create_final_pass_task(lines[i - 1], start, end, grp, wait_task);
        num_tasks++;
    }
    // Start the first task in each line
    for (int i = 0; i < num_div - 1; i++) {
        if (lines[i].first_task_)
            global_executor(lines[i].first_task_);
    }

    // Wait for all the task to complete
    wait_task = chained_task();
    wait(wait_grp);
}

} // namespace scan_auto_impl

/**
 * @brief      Special scan partitioning algorithm
 *
 * This is similar to the 'upfront' method, as it divides it work upfront. The division is made to
 * minimize the number of tasks, and therefore the number of joins that we need to do.
 *
 * Unlike other algorithms, to implement a scan, one cannot fully parallelize the work. The
 * dependencies between the tasks will create unbalanced work graphs.
 *
 * The general idea is the following (See
 * https://en.wikipedia.org/wiki/Prefix_sum#/media/File:Prefix_sum_16.svg):
 *  - divide the range into a power-of-two intervals
 *  - run the initial phase for each interval
 *  - perform initial set of joins
 *  - perform the second set of joins
 *  - run the final phase for each interval
 *
 * The algorithm creates tasks for all these, aggregate them into a task graph, and then runs the
 * graph.
 *
 * This only works for random-access iterators.
 */
template <typename RandomIt, typename WorkType>
inline void auto_partition_work_scan(
        RandomIt first, int n, WorkType& work, task_group grp, int granularity) {
    scan_auto_impl::algo(first, n, work, grp, granularity);
}

} // namespace detail
} // namespace concore