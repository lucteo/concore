#pragma once

namespace concore {

inline namespace v1 {

/**
 * @brief      The method of dividing the work for concurrent algorithms on ranges
 *
 * Using this would provide a hint the conc_for or conc_reduce algorithms on how to partition the
 * input data.
 *
 * The implementation of the algorithms may choose not to follow the specified method. Typically the
 * default method (auto_partition) works good enough, so most users don't need to change this.
 */
enum class partition_method {
    /**
     * Automatically partitions the data, trying to maximize locality.
     *
     * This method tries to create as many tasks as needed to fill the available workers, but tries
     * not to split the work too much to reduce locality. If only one worker is free to do work,
     * this method tries to put all the iterations in the task without splitting the work.
     *
     * Whenever new workers can take tasks, this method tries to ensure that the furthest away
     * elements are taken from the current processing.
     *
     * This method tries as much as possible to keep all the available workers busy, hopefully to
     * finish this faster. It works well if different iterations take different amounts of time
     * (work is not balanced).
     *
     * It uses spawning to divide the work. Can be influenced by the granularity level.
     *
     * This is the default method for random-access iterators.
     */
    auto_partition,
    /**
     * Partitions the data upfront.
     *
     * Instead of partitioning the data on the fly lie the auto_partiion method, this will partition
     * the data upfront, creating as many tasks as needed to cover all the workers. This can
     * minimize the task management, but it doesn't necessarily to ensure that all the workers have
     * tasks to do, especially in unbalanced workloads.
     *
     * Locality is preserved when splitting upfront.
     *
     * This method only works for random-access iterators.
     */
    upfront_partition,
    /**
     * Partitions the iterations as it advances through them.
     *
     * This partition tries to create a task for each iteration (or, if granularity is > 1, for a
     * number of iterations), and the tasks are created as the algorithm progresses. Locality is not
     * preserved, as nearby elements typically end up on different threads. This method tries to
     * always have tasks to be executed. When a task finished, a new task is spawned.
     *
     * This method works for forward iterators.
     *
     * This is the default method for non-random-access iterators.
     */
    iterative_partition,
    /**
     * Naive partition.
     *
     * This creates a task for each iteration (or, depending of granularity, on each group of
     * iterations). If there are too many elements in the given range, this can spawn too many
     * tasks.
     *
     * Does not preserve locality, but is ensures that the tasks are filling up the worker threads
     * in the best possible way.
     */
    naive_partition,
};

/**
 * @brief      Hints to alter the behavior of a @ref conc_for or @ref conc_reduce algorithms
 *
 * The hints in this structure can influence the behavior of the @ref conc_for and @ref conc_reduce
 * algorithms, but the algorithms can decide to ignore these hints.
 *
 * In general, the algorithms performs well on a large variety of cases, when the functions being
 * executed per element are not extremely fast. Therefore, manually giving hints to it is not
 * usually needed. If the operations are really fast, the user might want to play with the
 * granularity to ensure that the work unit is sufficiently large.
 *
 * @see partition_method
 */
struct partition_hints {
    //! The method of partitioning the input range
    partition_method method_ = partition_method::auto_partition;

    //! The granularity of the algorithm.
    //!
    //! When choosing how many iterations to handle in one task, this parameter can instruct the
    //! algorithm to not place less than the value here. This can be used when the iterations are
    //! really small, and the task management overhead can become significant.
    //! 
    //! Does not apply to the @ref partition_method::upfront_partition method
    int granularity_{1};
};

} // namespace v1
} // namespace concore
