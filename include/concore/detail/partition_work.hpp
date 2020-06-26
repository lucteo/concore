#pragma once

#include "concore/task_group.hpp"
#include "concore/spawn.hpp"
#include "concore/detail/platform.hpp"

#include <vector>

namespace concore {
namespace detail {

// Model of a work type:
//
// template <typename It>
// struct GenericWorkType {
//     void exec(It elem);
//     void exec(It first, It last);
//     static constexpr bool needs_join();
//     void join(GenericWorkType& rhs);
// };

/**
 * @brief      The default work partitioning algorithm, that tries to maximize locality.
 *
 * This will recursively divide the given range in half; spawn a task for handling the right half
 * and deal with the left half directly. In the absence of worker threads, the right hand tasks will
 * be executed at a much later point. In this context, to maximize locality, the left-hand tasks may
 * steal elements from the right side.
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
 * This only works for random-access iterators.
 */
template <typename RandomIt, typename WorkType>
inline void auto_partition_work(
        RandomIt first, int n, WorkType& work, task_group grp, int granularity) {
    if (n <= granularity) {
        // Cannot split anymore; just execute work
        work.exec(first, first + n);
        return;
    }

    // We assume we can't have more than 2^32 elements, so we can have max 32 half-way splits
    static constexpr int max_num_splits = 32;
    // Preallocate data on the stack
    int end_values[max_num_splits];
    std::atomic<int> split_values[max_num_splits];
    WorkType right_work_arr[max_num_splits];

    auto wait_grp = task_group::create(grp);

    // Iterate down, at each step splitting the range into half; stop when we reached the desired
    // granularity
    int level = 0;
    int end = n;
    while (end > granularity) {
        // Current interval: [*first, *first+end)

        // Spawn a task for the second half our our current interval
        int cur_mid = (end + 1) / 2;
        end_values[level] = end;
        split_values[level].store(cur_mid, std::memory_order_relaxed);
        std::atomic<int>& split = split_values[level];
        WorkType& right_work = right_work_arr[level];
        right_work = work;
        auto fun_second_half = [first, end, &split, &right_work, grp, granularity]() {
            // Pick up where the first part ended
            int cur_split = split.load(std::memory_order_relaxed);
            while (cur_split < end) {
                if (split.compare_exchange_weak(cur_split, -1, std::memory_order_acq_rel))
                    break;
            }
            // Recursively divide the remaining interval
            if (cur_split < end)
                auto_partition_work(
                        first + cur_split, end - cur_split, right_work, grp, granularity);
        };
        spawn(task{fun_second_half, wait_grp});

        // Now, handle in the current task the first half of the range
        level++;
        end = cur_mid;
    }
    level--;
    int max_level = level;

    // Now, left-to-right start executing the given functor
    // At each step, update the corresponding atomic to make sure other tasks are not executing the
    // same tasks If right-hand tasks did not start, steal iterations from them
    std::exception_ptr thrown_exception;
    try {
        int our_max = end;
        int i = 0;
        while (i < n) {
            // Run as many iterations as we can
            work.exec(first + i, first + our_max);
            i = our_max;
            if (our_max >= n)
                break;
            // If we can, try to steal some more from the right-hand task
            int lvl_end = end_values[level];
            int steal_end = std::min(our_max + granularity, lvl_end);
            int old_val = our_max;
            if (!split_values[level].compare_exchange_strong(
                        old_val, steal_end, std::memory_order_acq_rel))
                break;
            our_max = steal_end;
            // If we consumed everything from the right-side, move one level up
            if (our_max >= lvl_end)
                level--;
        }        
    }
    catch(...) {
        thrown_exception = std::current_exception();
        wait_grp.cancel();
    }

    // Wait for all the spawned tasks to be completed
    wait(wait_grp);

    if (thrown_exception)
        std::rethrow_exception(thrown_exception);

    // Join all the work items
    if (work.needs_join()) {
        for (int l = max_level; l >= 0; l--)
            work.join(right_work_arr[l]);
    }
}

/**
 * @brief      Tries to partition the work upfront
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
 * This only works for random-access iterators.
 */
template <typename RandomIt, typename WorkType>
inline void upfront_partition_work(RandomIt first, int n, WorkType& work, task_group grp) {
    auto wait_grp = task_group::create(grp);

    auto& tsys = detail::get_task_system();
    int num_tasks = tsys.num_worker_threads() * 2;

    int num_iter = num_tasks < n ? num_tasks : n;
    std::vector<WorkType> work_objs;

    if (work.needs_join() && num_iter > 1) {
        work_objs.resize(num_iter-1, work);
    }

    if (num_tasks < n) {
        for (int i = 0; i < num_tasks; i++) {
            auto start = first + (n * i / num_tasks);
            auto end = first + (n * (i + 1) / num_tasks);
            auto& work_obj = (work.needs_join() && i>0) ? work_objs[i-1] : work;
            spawn(task{[&work_obj, start, end] { work_obj.exec(start, end); }, wait_grp});
        }
    } else {
        for (int i = 0; i < n; i++) {
            auto& work_obj = (work.needs_join() && i>0) ? work_objs[i-1] : work;
            spawn(task{[&work_obj, first, i] { work_obj.exec(first + i); }, wait_grp});
        }
    }        

    // Wait for all the spawned tasks to be completed
    wait(wait_grp);

    // Join all the work items
    if (work.needs_join()) {
        for ( auto& w: work_objs)
            work.join(w);
    }
}

//! Helper class that can spawn "next" work for a range of elements. Uses locking to access the
//! iterators.
template <typename It, typename WorkType>
struct iterative_spawner {
    It first_;
    It last_;
    task_group grp_;
    spin_mutex bottleneck_;

    iterative_spawner(It first, It last, task_group grp)
        : first_(first)
        , last_(last)
        , grp_(grp) {}

    void spawn_task_1(WorkType& work, bool cont = false) {
        // Atomically take the first element from our range
        It it = take_1();
        if (it != last_) {
            auto t = task(
                    [this, it, &work]() {
                        work.exec(it);
                        spawn_task_1(work, true);
                    },
                    grp_);
            spawn(std::move(t), !cont);
        }
    }

    void spawn_task_n(WorkType& work, int count, bool cont = false) {
        // Atomically take the first elements from our range
        auto itp = take_n(count);
        auto begin = itp.first;
        auto end = itp.second;
        if (begin != end) {
            auto next = begin;
            next++;
            // Spawn a task that runs the ftor for the obtained range and spawns the next task
            auto t = task(
                    [this, begin, end, &work, count]() {
                        work.exec(begin, end);
                        spawn_task_n(work, count, true);
                    },
                    grp_);
            spawn(std::move(t), !cont);
        }
    }

    It take_1() {
        std::lock_guard<spin_mutex> lock(bottleneck_);
        return first_ == last_ ? last_ : first_++;
    }

    std::pair<It, It> take_n(int count) {
        std::lock_guard<spin_mutex> lock(bottleneck_);
        It begin = first_;
        It end = begin;
        for (int i = 0; i < count && end != last_; i++)
            end++;
        first_ = end;
        return std::make_pair(begin, end);
    }
};

/**
 * @brief      Partitions the work iteratively, as tasks get executed
 *
 * This will attempt to keep N tasks in flight, proportional with the number of workers we have. As
 * tasks are finished, this will spawn other tasks.
 *
 * As two tasks can finish at the same time, the access to the range that remains to be covered is
 * protected with a spin mutex.
 * 
 * This may not be as locality-aware as it can be, as consecutive elements are taken by different
 * threads. This tries to limit the number of tasks that are in flight (as opposed to naive
 * implementation).
 * 
 * This can work for forward iterators too.
 */
template <typename It, typename WorkType>
inline void iterative_partition_work(
        It first, It last, WorkType& work, task_group grp, int granularity) {
    auto wait_grp = task_group::create(grp);

    auto& tsys = detail::get_task_system();
    int num_tasks = tsys.num_worker_threads() * 2;

    std::vector<WorkType> work_objs;
    if (work.needs_join()) {
        work_objs.resize(num_tasks-1, work);
    }

    // Spawn the right number of tasks; each task will take 1 or more elements.
    // After completion, a task will spawn the next task
    iterative_spawner<It, WorkType> spawner(first, last, wait_grp);
    if (granularity <= 1) {
        for (int i = 0; i < num_tasks; i++) {
            auto& work_obj = (work.needs_join() && i>0) ? work_objs[i-1] : work;
            spawner.spawn_task_1(work_obj);
        }
    } else {
        for (int i = 0; i < num_tasks; i++) {
            auto& work_obj = (work.needs_join() && i>0) ? work_objs[i-1] : work;
            spawner.spawn_task_n(work_obj, granularity);
        }
    }
    // Wait for all the spawned tasks to be completed
    wait(wait_grp);

    // Join all the work items
    if (work.needs_join()) {
        for ( auto& w: work_objs)
            work.join(w);
    }
}

/**
 * @brief      Naive partition of work; a task for each chunk of work.
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
template <typename It, typename WorkType>
inline void naive_partition_work(
        It first, It last, WorkType& work, task_group grp, int granularity) {
    auto wait_grp = task_group::create(grp);

    if (granularity <= 1) {
        for (; first != last; first++) {
            spawn(task{[&work, first]() { work.exec(first); }, wait_grp});
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
            spawn(task{[&work, it, ite]() { work.exec(it, ite); }, wait_grp});

            it = ite;
        }
    }
    // Wait for all the spawned tasks to be completed
    wait(wait_grp);
}

} // namespace detail
} // namespace concore