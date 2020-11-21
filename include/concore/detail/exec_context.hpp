#pragma once

#include "concore/task.hpp"
#include "concore/profiling.hpp"
#include "concore/low_level/semaphore.hpp"
#include "concore/data/concurrent_queue.hpp"
#include "concore/detail/worker_tasks.hpp"
#include "concore/detail/task_priority.hpp"

#include <array>
#include <vector>
#include <thread>
#include <atomic>
#include <utility>

namespace concore {

inline namespace v1 {
class task;
class task_group;
struct init_data;
} // namespace v1

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

//! The task system, corresponding to a global executor.
//! This will create a set of worker threads corresponding to the number of cores we have on the
//! system, and each worker is capable of executing tasks.
class exec_context {
public:
    explicit exec_context(const init_data& config);
    ~exec_context();

    exec_context(const exec_context&) = delete;
    exec_context& operator=(const exec_context&) = delete;

    exec_context(exec_context&&) = delete;
    exec_context& operator=(exec_context&&) = delete;

    void enqueue(task&& t, task_priority prio = task_priority::normal);

    template <int P, typename T>
    void enqueue(T&& t) {
        CONCORE_PROFILING_FUNCTION();
        static_assert(P < num_priorities, "Invalid task priority");

        // Push the task in the global queue, corresponding to the given prio
        on_task_added();
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

    //! Wait until the given task group is not active anymore.
    //! This is going to be a busy wait, meaning that the caller will try to execute tasks.
    //! We hope that, this way we'll make progress towards finishing early.
    void busy_wait_on(task_group& grp);

    //! Called when spanning tasks and waiting for them to ensure we have a worker_thread_data.
    //! This is used when spawn_and_wait is called outside of our workers. If possible, we prepare
    //! a worker_data from a reserved slot, and return it. All the tasks spawned will be added to
    //! the returned worker_thread_data.
    //!
    //! Note that this can return null, if all slots are busy, of if we already have a worker on the
    //! current thread.
    worker_thread_data* enter_worker();
    //! Called at the end of spawn_and_wait, after waiting, to release the worker slot.
    //! Should be paired 1:1 with enter_worker();
    void exit_worker(worker_thread_data* worker_data);

    //! Called to attach the current thread as a worker to the execution context.
    //! The current function will not terminate until the execution context is destroyed.
    void attach_worker();

    //! Returns the number of worker threads we initially created
    int num_worker_threads() const { return count_; }

    //! Tests if there are tasks currently executing in our task system. This also counts any
    //! external threads that also join the task system.
    bool is_active() const { return num_tasks_.load() > 0 || num_active_workers_.load() > 0; }

    //! Returns the number of active tasks that are tracked by the task system. This includes the
    //! tasks that are currently executing, and does not include the tasks on the waiting lists of
    //! structures like serialiers. It also doesn't include the tasks spawned into the local worker
    //! queues.
    int num_active_tasks() const { return num_tasks_.load(); }

private:
    //! A task queue type
    using task_queue = concurrent_queue<task>;

    //! The number of worker threads that we should have
    const int count_;
    //! We reserve some extra slots for others threads that could temporary join our task system
    const int reserved_slots_;

    //! The data for each worker thread
    std::vector<worker_thread_data> workers_data_;

    //! Reserved slots, for external threads that call spawn_and_wait
    std::vector<worker_thread_data> reserved_worker_slots_;
    //! The number of extra slots that are currently in use; between [0, reserved_slots_]
    std::atomic<int> num_active_extra_slots_{0};

    //! The global task queue for each priority.
    //! We store here all the globally enqueued tasks
    std::array<task_queue, num_priorities> enqueued_tasks_;
    //! The number of tasks enqueued in the global queue (across all prios)
    std::atomic<int> num_global_tasks_{0};

    //! Flag used to announce the shutting down of the task system
    std::atomic<bool> done_{false};

    //! The number of tasks that we currently have enqueued in the task system
    mutable std::atomic<int> num_tasks_{0};

    //! The number of active workers; this allows us to quickly check if there is any work in the
    //! system.
    mutable std::atomic<int> num_active_workers_{0};

    //! The run procedure for a worker thread
    void worker_run(worker_thread_data& worker_data);

    //! Tries to extract a task and execute it. Returns false if couldn't extract a task
    bool try_extract_execute_task(worker_thread_data& worker_data);

    //! Puts the worker to sleep if the `done_` flag is not set
    void try_sleep(worker_thread_data& worker_data);

    //! Called before going to sleep to wait a bit and check for any incoming tasks.
    //! Returns true if we can safely go to sleep.
    bool before_sleep(worker_thread_data& worker_data);

    //! Called when adding a new task to wakeup the workers
    void wakeup_workers();

    //! Execute the given task
    void execute_task(task& t) const;

    //! Called whenever a worker becomes active
    void on_worker_active() const;
    //! Called whenever a worker becomes inactive
    void on_worker_inactive() const;

    //! Called when a task was added in the task system
    void on_task_added() const;
    //! Called when a task was removed from the task system (before execution)
    void on_task_removed() const;
};
} // namespace detail

} // namespace concore