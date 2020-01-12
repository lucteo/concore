#pragma once

#include "../task.hpp"
#include "../profiling.hpp"
#include "../low_level/semaphore.hpp"
#include "../data/concurrent_queue.hpp"

#include <array>
#include <vector>
#include <thread>
#include <atomic>

namespace concore {

namespace detail {

//! The possible priorities of tasks, as handled by the global executor
enum class task_priority {
    critical,   //! Critical priority; we better execute this tasks asap
    high,       //! High-priority tasks
    normal,     //! Tasks with normal priority
    low,        //! Tasks with low priority
    background, //! Background tasks; execute then when not doing something else
};

constexpr int num_priorities = 5;

//! The task system, corresponding to a global executor.
//! This will create a set of worker threads corresponding to the number of cores we have on the
//! system, and each worker is capable of executing tasks.
class task_system {
public:
    task_system();
    ~task_system();

    //! Get the instance of the class -- this is a singleton
    static task_system& instance() {
        static task_system inst;
        return inst;
    }

    template <int P, typename T>
    void enqueue(T&& t) {
        CONCORE_PROFILING_FUNCTION();
        static_assert(P < num_priorities, "Invalid task priority");

        // Push the task to the corresponding task queue (priority + round robin)
        int worker_idx = (index_++) % count_;
        task_queues_[P][worker_idx].push(std::forward<T>(t));

        // Wake up the worker if needed
        if (workers_data_[worker_idx].num_tasks_++ == 0)
            workers_data_[worker_idx].has_data_.signal();
    }

private:
    //! Structure containing the data for a worker thread
    struct worker_thread_data {
        std::thread thread_;
        semaphore has_data_;
        std::atomic<int> num_tasks_{0};
    };

    //! A task queue type
    using task_queue = concurrent_queue<task>;

    //! The number of worker threads that we should have
    const int count_{static_cast<int>(std::thread::hardware_concurrency())};
    //! The amount of iterations a thread will check for tasks before going to sleep
    const int spin_{std::max(64, count_)};

    //! The data for each worker thread
    std::vector<worker_thread_data> workers_data_{static_cast<size_t>(count_)};
    //! The queues of tasks for each priority, for each worker thread
    std::array<std::vector<task_queue>, num_priorities> task_queues_;
    //! Index used to do a round-robin through the worker threads when enqueueing tasks
    std::atomic<unsigned> index_{0};
    //! Flag used to announce the shutting down of the task system
    std::atomic_bool done_{false};

    //! The run procedure for a worker thread
    void worker_run(int worker_idx);

    //! Tries to extract a task and execute it. Returns false if couldn't extract a task
    bool execute_task(int worker_idx);

    //! Puts the worker to sleep if the `done_` flag is not set
    void try_sleep(int worker_idx);
};
} // namespace detail

} // namespace concore