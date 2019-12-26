#pragma once

#include <condition_variable>
#include <mutex>
#include <chrono>

using namespace std::chrono_literals;

//! Structure used to wait for all the tasks enqueued to be completed.
//!
//! The intended usage scenario:
//!     - each task has a reference to this object
//!     - upon completion, each task calls `task_finished`
//!     - the main thread calls `wait_for_all` to wait for all the tasks
//!         - this will return when all the tasks are completed, or whenever there is a timeout
//!         - the method returns true if all tasks were completed
//!
//! The `wait_for_all` method provides a timeout, to ensure that it doesn't block forever in the
//! case of bugs.
struct task_countdown {
    task_countdown(int num_tasks)
        : tasks_remaining_(num_tasks) {}

    //! Called by every task to announce that the task is completed
    void task_finished() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            tasks_remaining_--;
        }
        cond_.notify_all();
    }

    //! Called by the main thread to wait for all tasks to complete.
    //! Returns true if all tasks were completed, or false, if there is a timeout.
    bool wait_for_all(std::chrono::milliseconds timeout = 1000ms) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cond_.wait_for(lock, timeout, [this]() { return tasks_remaining_ == 0; });
    }

private:

    int tasks_remaining_;
    std::condition_variable cond_;
    std::mutex mutex_;
};
