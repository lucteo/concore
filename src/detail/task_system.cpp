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
    // First, try the high-prio tasks
    for (auto& qvec : task_queues_) {
        // Starting from our queue, try to get a task
        // If our queue is empty, we may be getting a task from another queue
        for (int i = 0; i != spin_ / count_; i++) {
            int idx = (worker_idx + i) % count_;
            if (qvec[idx].try_pop(t)) {
                // Decrement the number of existing tasks
                workers_data_[idx].num_tasks_--;

                // Found a task; run it
                try {
                    t();
                } catch (...) {
                }
                return true;
            }
        }
    }
    return false;
}

void task_system::try_sleep(int worker_idx) {
    // Early check to avoid entering the wait
    if (done_ || workers_data_[worker_idx].num_tasks_.load() > 0)
        return;
    workers_data_[worker_idx].has_data_.wait();
}

} // namespace detail
} // namespace concore