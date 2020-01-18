#pragma once

#include "task_countdown.hpp"

#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>

using namespace std::chrono_literals;

//! Simple test to ensure that the executor can execute a simple task
template <typename E>
inline void test_can_execute_a_task(E executor) {
    CONCORE_PROFILING_FUNCTION();

    std::atomic<int> val{0};
    // Enqueue a task that changes 'val'
    auto f = [&val]() { val = 1; };
    executor(f);
    // Wait until the value changes
    while (val.load() == 0)
        std::this_thread::sleep_for(1ms);
    // Ensure that the value is properly changed
    REQUIRE(val.load() == 1);
}

//! Test that ensures that an executor can execute multiple tasks
template <typename E>
void test_can_execute_multiple_tasks(E executor) {
    CONCORE_PROFILING_FUNCTION();

    constexpr int num_tasks = 10;
    task_countdown tc{num_tasks};
    bool results[num_tasks];

    // Enqueue the tasks
    for (int i = 0; i < num_tasks; i++)
        executor([&, i]() {
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

//! Test that the tasks do run in parallel with the given executor.
template <typename E>
void test_tasks_do_run_in_parallel(E executor) {
    CONCORE_PROFILING_FUNCTION();

    // This only works if we have multiple cores
    if (std::thread::hardware_concurrency() < 4)
        return;

    std::srand(std::time(0));

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
            executor([&, i]() {
                // Record the order we have at the start of the task
                starts[starts_end_idx++] = i;
                // Randomly wait a bit of time -- not all tasks have the same length
                int rnd = 1 + std::rand() % 6; // generates a number in range [1..6]
                std::this_thread::sleep_for(std::chrono::milliseconds(rnd));
                // Record the order we have at the task finish
                finishes[finishes_end_idx++] = i;
                tc.task_finished();
            });

        // Wait for all the tasks to complete
        REQUIRE(tc.wait_for_all());

        // Ensure that there is a mismatch between the start order and finish order
        for (int i = 0; i < num_tasks; i++)
            if (starts[i] != finishes[i]) {
                SUCCEED("Found parallel task execution");
                return;
            }
    }
    // fail
    REQUIRE(!"could not find a parallel task execution");
}
