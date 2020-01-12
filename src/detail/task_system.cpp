#include "concore/detail/task_system.hpp"

namespace concore {
namespace detail {

task_system::task_system() {
    CONCORE_PROFILING_INIT();
    CONCORE_PROFILING_FUNCTION();
    // Prepare all the queues (for all prios, and for all workers)
    for (auto& qvec : task_queues_) {
        std::vector<task_queue> newVec{static_cast<size_t>(count_)};
        qvec.swap(newVec);
    }
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

void task_system::worker_run(int worker_idx) {
    CONCORE_PROFILING_SETTHREADNAME("concore_worker");
    while (true) {
        if (done_)
            return;

        if (!execute_task(worker_idx)) {
            try_sleep(worker_idx);
        }
    }
}

bool task_system::execute_task(int worker_idx) {
    CONCORE_PROFILING_FUNCTION();
    task t;

    // TODO: take tasks from the corresponding worker queue

    // Try taking tasks from the global queue
    for (auto& q : enqueued_tasks_) {
        if (q.try_pop(t)) {
            // Decrement the number of existing tasks
            num_global_tasks_--;

            // Found a task; run it
            try {
                t();
            } catch (...) {
            }
            return true;
        }
    }

    return false;
}

void task_system::try_sleep(int worker_idx) {
    {
        CONCORE_PROFILING_SCOPE_N("before sleep");

        // Spin for a bit, in the hope that new tasks are added to the system
        // We hope that we avoid going to sleep just to be woken up immediately
        spin_backoff spinner;
        constexpr int new_active_wait_iterations = 8;
        for (int i = 0; i < new_active_wait_iterations; i++) {
            if (num_global_tasks_.load() > 0 || workers_data_[worker_idx].num_tasks_.load() > 0 ||
                    done_)
                return;
            spinner.pause();
        }

        // Nothing to do. Go to sleep. Mark workers_busy_ as false
        // We do a CAS here to avoid all workers going to sleep at the same time, just when new work
        // is enqueued.
        bool old = workers_busy_.load(std::memory_order_acquire);
        if (!workers_busy_.compare_exchange_strong(old, false, std::memory_order_acq_rel))
            return;
    }
    workers_data_[worker_idx].has_data_.wait();
}

void task_system::wakeup_workers() {
    for (int i = 0; i < count_; i++)
        workers_data_[i].has_data_.signal();
}

} // namespace detail
} // namespace concore