#pragma once

#include "task.hpp"

#include <deque>
#include <array>
#include <vector>
#include <thread>
#include <atomic>

namespace concore {

inline namespace v1 {

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

//! A task queue. Implements a queue of tasks, that can be accessed concurrently from different
//! threads. One pushes new tasks at the back, and always pops at the front.
class task_queue {
public:
    //! Try to pop one task from the front of the queue. Returns false if somebody else tries to
    //! read/write the queue.
    bool try_pop(task& t) {
        std::unique_lock<std::mutex> lock{mutex_, std::try_to_lock};
        if (!lock || tasks_.empty())
            return false;
        t = std::move(tasks_.front());
        tasks_.pop_front();
        return true;
    }
    //! Pops one task from the front of the queue. If the queue is busy waits until the previous
    //! operation finishes.
    bool pop(task& t) {
        std::unique_lock<std::mutex> lock{mutex_};
        if (tasks_.empty())
            return false;
        t = std::move(tasks_.front());
        tasks_.pop_front();
        return true;
    }

    //! Try to push one element on the back of the queue. Returns false if somebody else tries to
    //! read/write the queue.
    template <typename T>
    bool try_push(T&& t) {
        {
            std::unique_lock<std::mutex> lock{mutex_, std::try_to_lock};
            if (!lock)
                return false;
            tasks_.emplace_back(std::forward<T>(t));
        }
        ready_->notify_all();
        return true;
    }
    //! Pushes one element on the back of the queue. If the queue is busy waits until the previous
    //! operation finishes.
    template <typename T>
    void push(T&& t) {
        {
            std::unique_lock<std::mutex> lock{mutex_};
            tasks_.emplace_back(std::forward<T>(t));
        }
        ready_->notify_all();
    }

    //! Sets the condition variable that is notified whenever a new task is pushed to the queue.
    void set_ready_cv(std::condition_variable& cv) { ready_ = &cv; }

private:
    //! The queue of tasks that we are encapsulating
    std::deque<task> tasks_;
    //! Mutex used to protect the access in the queue
    std::mutex mutex_;
    //! Pointer to the conditional variable that indicates whether the queue has work
    std::condition_variable* ready_{nullptr};
};

class task_system {
public:
    task_system() {
        // Prepare all the queues (for all prios, and for all workers)
        for (auto& qvec : task_queues_) {
            std::vector<task_queue> newVec{static_cast<size_t>(count_)};
            qvec.swap(newVec);
            for (int i = 0; i < count_; i++) {
                qvec[i].set_ready_cv(workers_data_[i].ready_);
            }
        }
        // Start the worker threads
        for (int i = 0; i < count_; i++)
            workers_data_[i].thread_ = std::thread([this, i]() { worker_run(i); });
    }
    ~task_system() {
        // Set the flag to mark shut down, and wake all the threads
        done_ = true;
        for (auto& worker_data : workers_data_)
            worker_data.ready_.notify_all();
        // Wait for all the threads to finish
        for (auto& worker_data : workers_data_)
            worker_data.thread_.join();
    }

    //! Get the instance of the class -- this is a singleton
    static task_system& instance() {
        static task_system inst;
        return inst;
    }

    template <int P, typename T>
    void enqueue(T&& t) {
        static_assert(P < num_priorities, "Invalid task priority");
        int cur_idx = index_++;

        // Find a queue, and try to push the task on it. If we cannot do that, try another queue
        for (int i = 0; i != spin_ / count_; i++) {
            if (task_queues_[P][(cur_idx + i) % count_].try_push(std::forward<T>(t)))
                return;
        }

        // All queues are busy; just enqueue to the corresponding queue
        task_queues_[P][cur_idx % count_].push(std::forward<T>(t));
    }

private:
    //! Structure corresponding to a worker thread
    struct worker_thread_data {
        std::mutex mutex_;
        std::condition_variable ready_;
        std::thread thread_;
    };

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
    void worker_run(int worker_idx) {
        while (true) {
            if (done_)
                return;

            if (!execute_task(worker_idx)) {
                try_sleep(worker_idx);
            }
        }
    }

    //! Tries to extract a task and execute it. Returns false if couldn't extract a task
    bool execute_task(int worker_idx) {
        task t;
        // First, try the high-prio tasks
        for (auto& qvec : task_queues_) {
            // Starting from our queue, try to get a task
            for (int i = 0; i != spin_ / count_; i++) {
                if (qvec[(worker_idx + i) % count_].try_pop(t)) {
                    // Found a task; run it
                    t();
                    return true;
                }
            }
        }
        return false;
    }

    //! Puts the worker to sleep if the `done_` flag is not set
    void try_sleep(int worker_idx) {
        std::unique_lock<std::mutex> lock{workers_data_[worker_idx].mutex_};
        if (!done_)
            workers_data_[worker_idx].ready_.wait(lock);
    }
};

//! Structure that defines an executor for a given priority
template <task_priority P = task_priority::normal>
struct executor_with_prio {
    void operator()(task t) const { task_system::instance().enqueue<int(P)>(std::move(t)); }
};

} // namespace detail

//! The global task executor. This will enqueue a task with a "normal" priority in the system, and
//! whenever we have a core available to execute it, it will be executed.
constexpr auto global_executor = detail::executor_with_prio<detail::task_priority::normal>{};

//! Task executor that enqueues tasks with "critical" priority.
constexpr auto global_executor_critical_prio =
        detail::executor_with_prio<detail::task_priority::critical>{};
//! Task executor that enqueues tasks with "high" priority.
constexpr auto global_executor_high_prio =
        detail::executor_with_prio<detail::task_priority::high>{};
//! Task executor that enqueues tasks with "normal" priority. Same as `global_executor`.
constexpr auto global_executor_normal_prio =
        detail::executor_with_prio<detail::task_priority::normal>{};
//! Task executor that enqueues tasks with "low" priority.
constexpr auto global_executor_low_prio = detail::executor_with_prio<detail::task_priority::low>{};
//! Task executor that enqueues tasks with "background" priority.
constexpr auto global_executor_background_prio =
        detail::executor_with_prio<detail::task_priority::background>{};

} // namespace v1
} // namespace concore