#pragma once

#include "../task.hpp"
#include "../profiling.hpp"
#include "../low_level/semaphore.hpp"
#include "../data/concurrent_queue.hpp"
#include "worker_tasks.hpp"

#include <array>
#include <vector>
#include <thread>
#include <atomic>

namespace concore {

namespace detail {

//! Structure containing the data for a worker thread
struct worker_thread_data {
    using task_queue = concurrent_queue<task>;

    enum worker_state {
        idle = 0, //!< We don't have any tasks and we are sleeping
        waiting,  //!< No tasks, but we are spinning in the hope to catch a task
        running,  //!< We have some tasks, or we think we have some tasks to execute
    };

    //! The thread object for this worker
    std::thread thread_;
    //! The state of the worker
    std::atomic<int> state_{running};
    //! Semaphore used to signal when the worker has data, or some processing to do
    binary_semaphore has_data_;
    //! The stack of tasks spawned by this worker
    worker_tasks local_tasks_;
};

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

        // Push the task in the global queue, corresponding to the given prio
        enqueued_tasks_[P].push(std::forward<T>(t));
        num_global_tasks_++;
        wakeup_workers();
    }

    //! Tries to spawn the given task, adding it to the local work queue for the current worker
    //! thread. If this is called from a non-worker thread, the tasks will be enqueued at the back
    //! of the global queue.
    //!
    //! If wake_workers is false, this will not attempt to wake other workers to try to steal the
    //! task.
    void spawn(task&& t, bool wake_workers = true);

private:
    //! A task queue type
    using task_queue = concurrent_queue<task>;

    //! The number of worker threads that we should have
    const int count_{static_cast<int>(std::thread::hardware_concurrency())};
    //! The amount of iterations a thread will check for tasks before going to sleep
    const int spin_{std::max(64, count_)};

    //! The data for each worker thread
    std::vector<worker_thread_data> workers_data_{static_cast<size_t>(count_)};

    //! The global task queue for each priority.
    //! We store here all the globally enqueued tasks
    std::array<task_queue, num_priorities> enqueued_tasks_;
    //! The number of tasks enqueued in the global queue (across all prios)
    std::atomic<int> num_global_tasks_{0};

    //! Flag used to announce the shutting down of the task system
    std::atomic<bool> done_{false};

    //! The run procedure for a worker thread
    void worker_run(int worker_idx);

    //! Tries to extract a task and execute it. Returns false if couldn't extract a task
    bool try_extract_execute_task(int worker_idx);

    //! Puts the worker to sleep if the `done_` flag is not set
    void try_sleep(int worker_idx);

    //! Called before going to sleep to wait a bit and check for any incoming tasks.
    //! Returns true if we can safely go to sleep.
    bool before_sleep(int worker_idx);

    //! Called when adding a new task to wakeup the workers
    void wakeup_workers();

    //! Execute the given task
    void execute_task(task& t) const;
};
} // namespace detail

} // namespace concore