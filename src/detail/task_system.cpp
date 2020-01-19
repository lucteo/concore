#include "concore/detail/task_system.hpp"

namespace concore {
namespace detail {

//! TLS pointer to the worker data. This way, if we are called from a worker thread we can interact
//! with the worker thread data
thread_local worker_thread_data* g_worker_data{nullptr};

task_system::task_system() {
    CONCORE_PROFILING_INIT();
    CONCORE_PROFILING_FUNCTION();
    // Start the worker threads
    for (int i = 0; i < count_; i++)
        workers_data_[i].thread_ = std::thread([this, i]() { worker_run(i); });
}
task_system::~task_system() {
    CONCORE_PROFILING_FUNCTION();
    // Set the flag to mark shut down, and wake all the threads
    done_ = true;
    for (auto& worker_data : workers_data_)
        worker_data.has_data_.signal();
    // Wait for all the threads to finish
    for (auto& worker_data : workers_data_)
        worker_data.thread_.join();
}

void task_system::spawn(task&& t, bool wake_workers) {
    CONCORE_PROFILING_FUNCTION();
    worker_thread_data* data = g_worker_data;

    // If no worker thread data is stored on the current thread, this is not a worker thread, so we
    // should enqueue the task
    if (!data) {
        enqueue<int(task_priority::normal)>(std::forward<task>(t));
        return;
    }

    // Add the task to the worker's queue
    data->local_tasks_.push(std::forward<task>(t));

    // Wake up the workers
    if (wake_workers)
        wakeup_workers();
}

void task_system::worker_run(int worker_idx) {
    CONCORE_PROFILING_SETTHREADNAME("concore_worker");
    g_worker_data = &workers_data_[worker_idx];
    while (true) {
        if (done_)
            return;

        if (!try_extract_execute_task(worker_idx)) {
            try_sleep(worker_idx);
        }
    }
}

bool task_system::try_extract_execute_task(int worker_idx) {
    CONCORE_PROFILING_FUNCTION();
    task t;

    // Attempt to consume tasks from the local queue
    auto& worker_data = workers_data_[worker_idx];
    if (worker_data.local_tasks_.try_pop(t)) {
        execute_task(t);
        return true;
    }

    // Try taking tasks from the global queue
    for (auto& q : enqueued_tasks_) {
        if (q.try_pop(t)) {
            num_global_tasks_--;
            execute_task(t);
            return true;
        }
    }

    // Try stealing a task from another worker
    for (int i = 0; i < count_; i++) {
        if (i != worker_idx) {
            if (workers_data_[i].local_tasks_.try_steal(t)) {
                execute_task(t);
                return true;
            }
        }
    }

    return false;
}

void task_system::try_sleep(int worker_idx) {
    auto& data = workers_data_[worker_idx];
    data.state_.store(worker_thread_data::waiting);
    if (before_sleep(worker_idx)) {
        data.has_data_.wait();
    }
    data.state_.store(worker_thread_data::running);
}

bool task_system::before_sleep(int worker_idx) {
    CONCORE_PROFILING_FUNCTION();
    CONCORE_PROFILING_SET_TEXT_FMT(32, "%d", worker_idx);
    auto& data = workers_data_[worker_idx];

    data.state_.store(worker_thread_data::waiting);

    // Spin for a bit, in the hope that new tasks are added to the system
    // We hope that we avoid going to sleep just to be woken up immediately
    spin_backoff spinner;
    constexpr int new_active_wait_iterations = 8;
    for (int i = 0; i < new_active_wait_iterations; i++) {
        if (num_global_tasks_.load() > 0 || done_)
            return false;
        spinner.pause();
    }

    // Ok, so now we have to go to sleep
    int old = worker_thread_data::waiting;
    if (!data.state_.compare_exchange_strong(old, worker_thread_data::idle))
        return false; // somebody prevented us to go to sleep

    return true;
}

void task_system::wakeup_workers() {
    CONCORE_PROFILING_FUNCTION();

    // First try to wake up any worker that is in waiting state
    int num_idle = 0;
    for (int i = 0; i < count_; i++) {
        int old = worker_thread_data::waiting;
        if (workers_data_[i].state_.compare_exchange_strong(old, worker_thread_data::running)) {
            // Put a worker from waiting to running. That should be enough
            return;
        }

        if (old == worker_thread_data::idle)
            num_idle++;
    }
    // If we are here, it means that all our workers are either running or idle.
    // If a worker just switched from running to waiting, it should should be picking up the new
    // task. But we should still wake up one idle thread.
    if (num_idle > 0) {
        for (int i = 0; i < count_; i++) {
            int old = worker_thread_data::idle;
            if (workers_data_[i].state_.compare_exchange_strong(old, worker_thread_data::running)) {
                // CONCORE_PROFILING_SCOPE_N("waking")
                // CONCORE_PROFILING_SET_TEXT_FMT(32, "%d", i);
                workers_data_[i].has_data_.signal();
                return;
            }
        }
    }
    // If we are here, it means that all workers are woken up.
}

void task_system::execute_task(task& t) const {
    CONCORE_PROFILING_FUNCTION();
    try {
        t();
    } catch (...) {
    }
}

} // namespace detail
} // namespace concore