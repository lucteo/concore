#include <catch2/catch.hpp>
#include <concore/global_executor.hpp>
#include <concore/executor_type.hpp>

#include <atomic>
#include <thread>

using namespace std::chrono_literals;

//! Structure used to wait for all the tasks enqueued to be completed.
//! The intended usage scenario:
//!     - each task has a reference to this object
//!     - upon completion, each task calls `task_finished`
//!     - the main thread calls `wait_for_all` to wait for all the tasks
//!         - this will return when all the tasks are completed, or whenever there is a timeout
//!         - the method returns true if all tasks were completed
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

TEST_CASE("global_executor is copyable") {
    auto e1 = concore::global_executor;
    auto e2 = concore::global_executor;
    e2 = e1;
}

TEST_CASE("global_executor executes a task") {
    std::atomic<int> val{0};
    auto f = [&val]() { val = 1; };
    concore::global_executor(f);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(val.load() == 1);
}

TEST_CASE("global_executor executes all tasks") {
    constexpr int num_tasks = 10;
    task_countdown tc{num_tasks};
    bool results[num_tasks];

    // Create the tasks
    for (int i = 0; i < num_tasks; i++)
        concore::global_executor([&, i]() {
            results[i] = true;
            tc.task_finished();
        });

    // Wait for all the tasks to complete
    auto res = tc.wait_for_all();
    REQUIRE(res); // no timeout

    // Ensure that all tasks were executed
    for (int i = 0; i < num_tasks; i++)
        REQUIRE(results[i] == true);
}

TEST_CASE("global_executor's task completion is out-of-order") {
    constexpr int max_runs = 100;
    // We are not guaranteed to find an out-of-order execution pattern; so try multiple times.
    for (int k = 0; k < max_runs; k++) {
        constexpr int num_tasks = 10;
        task_countdown tc{num_tasks};

        // We store the order of execution in the results array
        int results[num_tasks];
        std::atomic<int> end_idx{0};

        // Create the tasks
        for (int i = 0; i < num_tasks; i++)
            concore::global_executor([&, i]() {
                results[end_idx++] = i;
                tc.task_finished();
            });

        // Wait for all the tasks to complete
        REQUIRE(tc.wait_for_all());

        // Ensure that the results are not ordered
        for (int i = 0; i < num_tasks; i++)
            if (results[i] != i) {
                SUCCEED("Found out-of-order execution");
                return;
            }
    }
    // fail
    REQUIRE(!"could not find an out-of-order execution");
}

TEST_CASE("global_executor runs tasks in parallel") {
    // This only works if we have multiple cores
    if (std::thread::hardware_concurrency() <= 2)
        return;

    constexpr int max_runs = 100;
    // We are not guaranteed to find an out-of-order execution pattern; so try multiple times.
    for (int k = 0; k < max_runs; k++) {
        constexpr int num_tasks = 10;
        task_countdown tc{num_tasks};

        // We store the start order and the finish order for all the tasks
        int starts[num_tasks];
        std::atomic<int> starts_end_idx{0};
        int finishes[num_tasks];
        std::atomic<int> finishes_end_idx{0};

        // Create the tasks
        for (int i = 0; i < num_tasks; i++)
            concore::global_executor([&, i]() {
                // Record the order we have at the start of the task
                starts[starts_end_idx++] = i;
                // Randomly wait a bit of time
                int rnd = 1 + rand() % 6; // generates a number in range [1..6]
                std::this_thread::sleep_for(std::chrono::milliseconds(rnd));
                // Record the order we have at the task finish
                finishes[finishes_end_idx++] = i;
                tc.task_finished();
            });

        // Wait for all the tasks to complete
        REQUIRE(tc.wait_for_all());

        // Ensure that the results are not ordered
        for (int i = 0; i < num_tasks; i++)
            if (starts[i] != finishes[i]) {
                SUCCEED("Found parallel task execution");
                return;
            }
    }
    // fail
    REQUIRE(!"could not find a parallel task execution");
}

TEST_CASE("global_executor executes tasks according to their prio") {
    // Run this multiple times, as execution order can be tricky
    constexpr int num_runs = 10;
    for (int k = 0; k < num_runs; k++) {
        // The executors for all the priorities that we have
        constexpr int num_prios = 5;
        concore::executor_t executors[num_prios] = {
                concore::global_executor_background_prio,
                concore::global_executor_low_prio,
                concore::global_executor_normal_prio,
                concore::global_executor_high_prio,
                concore::global_executor_critical_prio,
        };

        constexpr int num_tasks_per_prio = 30;
        constexpr int num_tasks = num_prios * num_tasks_per_prio;
        task_countdown tc{num_tasks};

        // We store the priorities of each task; the order will be consistent with the task
        // execution order.
        int task_prios[num_tasks];
        std::atomic<int> end_idx{0};

        // Create the tasks; start with the low prio ones
        for (int p = 0; p < num_prios; p++) {
            for (int i = 0; i < num_tasks_per_prio; i++)
                executors[p]([&, p]() {
                    task_prios[end_idx++] = p;
                    std::this_thread::sleep_for(1ms);
                    tc.task_finished();
                });
        }

        // Wait for all the tasks to complete
        REQUIRE(tc.wait_for_all());

        // printf("Result: ");
        // for (int i = 0; i < num_tasks; i++)
        //     printf("%d", task_prios[i]);
        // printf("\n");

        // Now check if the execution order based on the `task_prios` array.
        // In general, we look for a pattern like: "LLHHHHHHHHLLHLLLLLL". That is:
        //  - at the start, lower prio tasks can be executed
        //  - once a higher-prio task is executed, all higher-prio tasks are executed before
        //  lower-prio ones
        //  - however, at the end of execution we might have some interference from lower-prio tasks
        //  - that interference should be comparable with the number of cores
        //  - the rest of lower prio tasks are executed after the higher-prio ones

        for (int p = 0; p < num_prios; p++) {
            // Find the first execution of a task with priority `p`
            int idx = 0;
            while (task_prios[idx] != p) {
                idx++;
                REQUIRE(idx < num_tasks);
            }
            // Now, while we still have tasks at priority `p`, don't allow any lower-prio tasks
            int num_wrong = 0;
            int count = num_tasks_per_prio - 1;
            for (int i = idx + 1; i < num_tasks && count > 0; i++) {
                if (task_prios[i] < p)
                    num_wrong++;
                if (task_prios[i] == p)
                    count--;
            }
            REQUIRE(count == 0);
            REQUIRE(num_wrong <= std::thread::hardware_concurrency());
        }
    }
}
