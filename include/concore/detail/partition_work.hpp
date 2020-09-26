#pragma once

#include "concore/task_group.hpp"
#include "concore/spawn.hpp"
#include "concore/detail/platform.hpp"
#include "concore/detail/algo_utils.hpp"

#include <vector>
#include <array>
#include <memory>

namespace concore {
namespace detail {

// Model of a work type:
//
// template <typename It>
// struct GenericWorkType {
//     using iterator = It;
//     void exec(It first, It last);
//
//     // for reduce:
//     void join(conc_reduce_work& rhs);
// };

namespace auto_part {

template <typename WorkType, bool needs_join>
struct work_interval : std::enable_shared_from_this<work_interval<WorkType, needs_join>> {
    using iterator = typename WorkType::iterator;

    using ptr_type = std::shared_ptr<work_interval>;

    std::atomic<int> join_predecessors_;
    const iterator first_;
    const int count_;
    std::atomic<int> start_idx_;
    WorkType work_;
    const int granularity_;
    ptr_type parent_;
    ptr_type next_;

    work_interval(iterator first, int start_idx, int cnt, WorkType work, int granularity)
        : join_predecessors_(1)
        , first_(first)
        , count_(cnt)
        , start_idx_(start_idx)
        , work_(std::move(work))
        , granularity_(granularity)
        , parent_(nullptr)
        , next_(nullptr) {}

    void run(int start_idx = 0);
    void run_as_right();
    void release();
};

template <typename WorkType, bool needs_join>
void work_interval<WorkType, needs_join>::run(int start_idx) {
    auto first = first_ + start_idx;
    int n = count_ - start_idx;

    if (n <= granularity_) {
        // Cannot split anymore; just execute work
        work_.exec(first, first + n);
        return;
    }

    static constexpr int max_num_splits = 32;
    std::array<ptr_type, max_num_splits> right_intervals{};
    right_intervals.fill(nullptr);

    // Iterate down, at each step splitting the range into half; stop when we reached the desired
    // granularity
    int level = 0;
    int end = n;
    while (end > granularity_) {
        // Current interval: [first, first+end)

        // Create a task to handle the right side
        int start_right = (end + 1) / 2;
        auto right = std::make_shared<work_interval>(
                first_, start_idx + start_right, start_idx + end, work_, granularity_);
        right->join_predecessors_++;
        right_intervals[level] = right;

        // If we don't need join, spawn the task immediatelly
        if constexpr (!needs_join)
            spawn([right]() { right->run_as_right(); });

        // Recurse down in the left side of the interval
        level++;
        end = start_right;
    }
    level--;
    int max_level = level;

    if constexpr (needs_join) {
        // Set the connections between intervals, so that we join in the proper order
        // Larger intervals (levels closer to zero) will wait for smaller intervals to complete
        // As the connections are made, also start the tasks
        join_predecessors_ += max_level + 1;
        for (int l = 0; l < max_level; l++) {
            right_intervals[l + 1]->next_ = right_intervals[l];
            // pred = 3 means: its own work, parent, and next smaller interval
            right_intervals[l]->join_predecessors_.store(3, std::memory_order_relaxed);
        }
        for (int l = 0; l <= max_level; l++) {
            auto& cur = right_intervals[l];
            cur->parent_ = this->shared_from_this();
            spawn([cur]() { cur->run_as_right(); });
        }
    }

    std::exception_ptr thrown_exception;

    try {
        // Now, left-to-right start executing the work
        // At each step, update the corresponding atomic to make sure other tasks are not executing
        // the same tasks. If right-hand tasks did not start, steal iterations from them
        int our_max = end;
        int i = 0;
        while (i < n) {
            // Run as many iterations as we can
            work_.exec(first + i, first + our_max);
            i = our_max;
            if (our_max == n)
                break;
            // If we can, try to steal some more from the right-hand task
            work_interval& right = *right_intervals[level];
            int lvl_end = right.count_;
            int steal_end = std::min(our_max + granularity_, lvl_end);
            int old_val = our_max;
            if (!right.start_idx_.compare_exchange_strong(
                        old_val, steal_end, std::memory_order_acq_rel))
                break;
            our_max = steal_end;
            assert(our_max <= lvl_end);
            assert(our_max <= n);
            // If we consumed everything from the right-side, move one level up
            if (our_max == lvl_end)
                level--;
        }
    } catch (...) {
        thrown_exception = std::current_exception();
    }

    // Ensure that we release all the non-null levels
    // To prevent race conditions, this needs to be done after we've done touching 'work_'
    for (int lvl = max_level; lvl >= 0; lvl--) {
        right_intervals[lvl]->release();
        right_intervals[lvl].reset();
    }

    // If we have an exception, re-throw it
    if (thrown_exception)
        std::rethrow_exception(thrown_exception);

    // For deletion, we use shared_pointers to properly delete our work intervals whenever needed;
    // please note that we may have more interval objects than actually tasks. On exceptions, some
    // of the tasks will not run, but the objects still need to be cleaned up.

    // The logic for joins:
    //  - general considerations
    //      - typically it follows the reference counts of the shared_ptrs (if we don't have
    //      exceptions)
    //      - we use a join_predecessors_ counter to determine the number of predecessors
    //      - the join operation is typcailly the last operation that is done on an interval
    //  - we have join_predecessors_ of 3, plus the number of sub-intervals
    //      - 1 in the parent
    //      - one the task that is running
    //      - one the task that need to join before the current one (smaller right interval)
    //      - and also, each children keeps a reference to the parent
    //  - the joins are executed in the order of deleting the intervals, so we control it
    //  - we want that smaller intervals are joining before larger ones
}

template <typename WorkType, bool needs_join>
void work_interval<WorkType, needs_join>::run_as_right() {
    // Pick up where the previous part ended
    int cur_start = start_idx_.load(std::memory_order_relaxed);
    while (cur_start < count_) {
        if (start_idx_.compare_exchange_weak(cur_start, -1, std::memory_order_acq_rel))
            break;
    }
    // Run if there is remaining work to do
    if (cur_start < count_) {
        run(cur_start);
    }
    release();
}

template <typename WorkType, bool needs_join>
void work_interval<WorkType, needs_join>::release() {
    if constexpr (needs_join) {
        if (join_predecessors_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            // We are done with this interval; now it's the time to join the result in the parent
            // work
            std::exception_ptr thrown_exception;
            try {
                if (parent_)
                    parent_->work_.join(work_);
            } catch (...) {
                thrown_exception = std::current_exception();
            }

            // Release the parent and the next interval that needs to be joined in the parent
            // cppcheck-suppress duplicateCondition
            if (parent_)
                parent_->release();
            if (next_)
                next_->release();

            // If we have an exception, re-throw it
            if (thrown_exception)
                std::rethrow_exception(thrown_exception);

            // soon after this, the shared_ptr destructor will delete our object
        }
    }
}
} // namespace auto_part

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
template <bool needs_join, typename WorkType>
inline void auto_partition_work(typename WorkType::iterator first, int n, WorkType& work,
        task_group& grp, int granularity) {
    assert(task_group::current_task_group());
    auto all = std::make_shared<auto_part::work_interval<WorkType, needs_join>>(
            first, 0, n, std::move(work), granularity);
    all->run();
    wait(grp);
    work = std::move(all->work_);
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
template <bool needs_join, typename RandomIt, typename WorkType>
inline void upfront_partition_work(
        RandomIt first, int n, WorkType& work, task_group& wait_grp, int tasks_per_worker) {
    auto& tsys = detail::get_task_system();
    int num_tasks = tsys.num_worker_threads() * tasks_per_worker;

    int num_iter = num_tasks < n ? num_tasks : n;
    std::vector<WorkType> work_objs;

    if (needs_join && num_iter > 1)
        work_objs.resize(num_iter - 1, work);

    if (num_tasks < n) {
        for (int i = 0; i < num_tasks; i++) {
            auto start = first + (n * i / num_tasks);
            auto end = first + (n * (i + 1) / num_tasks);
            auto& work_obj = (needs_join && i > 0) ? work_objs[i - 1] : work;
            spawn(task{[&work_obj, start, end] { work_obj.exec(start, end); }, wait_grp});
        }
    } else {
        for (int i = 0; i < n; i++) {
            auto& work_obj = (needs_join && i > 0) ? work_objs[i - 1] : work;
            spawn(task{
                    [&work_obj, first, i] { work_obj.exec(first + i, first + i + 1); }, wait_grp});
        }
    }

    // Wait for all the spawned tasks to be completed
    wait(wait_grp);

    // Join all the work items
    if constexpr (needs_join) {
        for (auto& w : work_objs)
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
        , grp_(std::move(grp)) {}

    void spawn_task_1(WorkType& work, bool cont = false) {
        // Atomically take the first element from our range
        It it = take_1();
        if (it != last_) {
            auto t = task(
                    [this, it, &work]() {
                        work.exec(it, it_next(it));
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
template <bool needs_join, typename It, typename WorkType>
inline void iterative_partition_work(
        It first, It last, WorkType& work, task_group& wait_grp, int granularity) {
    auto& tsys = detail::get_task_system();
    int num_tasks = tsys.num_worker_threads() * 2;

    std::vector<WorkType> work_objs;
    if (needs_join && num_tasks > 1)
        work_objs.resize(num_tasks - 1, work);

    // Spawn the right number of tasks; each task will take 1 or more elements.
    // After completion, a task will spawn the next task
    iterative_spawner<It, WorkType> spawner(first, last, wait_grp);
    if (granularity <= 1) {
        for (int i = 0; i < num_tasks; i++) {
            auto& work_obj = (needs_join && i > 0) ? work_objs[i - 1] : work;
            spawner.spawn_task_1(work_obj);
        }
    } else {
        for (int i = 0; i < num_tasks; i++) {
            auto& work_obj = (needs_join && i > 0) ? work_objs[i - 1] : work;
            spawner.spawn_task_n(work_obj, granularity);
        }
    }
    // Wait for all the spawned tasks to be completed
    wait(wait_grp);

    // Join all the work items
    if constexpr (needs_join) {
        for (auto& w : work_objs)
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
 *
 * This method doesn't work in reduce scenarios.
 */
template <typename It, typename WorkType>
inline void naive_partition_work(
        It first, It last, WorkType& work, const task_group& wait_grp, int granularity) {
    if (granularity <= 1) {
        for (; first != last; first++) {
            spawn(task{[&work, first]() { work.exec(first, it_next(first)); }, wait_grp});
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
}

} // namespace detail
} // namespace concore