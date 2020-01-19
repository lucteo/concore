#include <catch2/catch.hpp>
#include <concore/spawn.hpp>
#include <concore/executor_type.hpp>

#include "test_common/common_executor_tests.hpp"
#include "test_common/task_countdown.hpp"

using namespace std::chrono_literals;

TEST_CASE("spawn_executor is copyable") {
    auto e1 = concore::spawn_executor;
    auto e2 = concore::spawn_executor;
    e2 = e1;
}

TEST_CASE("spawn_executor executes a task") { test_can_execute_a_task(concore::spawn_executor); }

TEST_CASE("spawn_executor executes all tasks") {
    test_can_execute_multiple_tasks(concore::spawn_executor);
}

void task_fun(task_countdown& tc, int n) {
    CONCORE_PROFILING_FUNCTION();
    // Each task spawns the new one
    if (n > 0)
        concore::spawn([&tc, n]() { task_fun(tc, n - 1); });
    tc.task_finished();
}

TEST_CASE("spawning tasks from existing tasks") {
    constexpr int num_tasks = 20;
    task_countdown tc{num_tasks};
    concore::spawn([&tc, num_tasks]() { task_fun(tc, num_tasks - 1); });

    // Wait for all the tasks to complete
    REQUIRE(tc.wait_for_all());
}

TEST_CASE("spawned tasks can be executed by multiple workers") {
    CONCORE_PROFILING_FUNCTION();

    // This only works if we have multiple cores
    if (std::thread::hardware_concurrency() < 4)
        return;

    // We are not guaranteed to find an out-of-order execution pattern; so try multiple times.
    constexpr int max_runs = 100;
    for (int k = 0; k < max_runs; k++) {
        constexpr int num_tasks = 20;

        int starts[num_tasks];
        int ends[num_tasks];
        std::atomic<int> counter{0};

        task_countdown tc{num_tasks};

        auto child_task = [&](int i) {
            starts[i] = counter++;
            std::this_thread::sleep_for(3ms);
            ends[i] = counter++;
            tc.task_finished();
        };

        // Start a task that spawn all the other tasks
        concore::spawn([&]() {
            for (int i = 0; i < num_tasks; i++)
                concore::spawn([i, &child_task]() { child_task(i); });
        });

        // Wait for all the tasks to complete
        REQUIRE(tc.wait_for_all());

        // Now check if the tasks were run serially
        for (int i = 0; i < num_tasks; i++) {
            if (starts[i] + 1 != ends[i]) {
                SUCCEED("Found parallel task execution");
                return;
            }
        }
    }
    // fail
    REQUIRE(!"could not find a parallel task execution");
}