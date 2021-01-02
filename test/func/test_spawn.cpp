#include <catch2/catch.hpp>
#include <concore/spawn.hpp>
#include <concore/init.hpp>
#include <concore/executor_type.hpp>

#include "test_common/common_executor_tests.hpp"
#include "test_common/task_countdown.hpp"

#include <array>

using namespace std::chrono_literals;

TEST_CASE("spawn_executor is copyable") {
    auto e1 = concore::spawn_executor{};
    auto e2 = concore::spawn_executor{};
    // cppcheck-suppress redundantInitialization
    // cppcheck-suppress unreadVariable
    e2 = e1;
}

TEST_CASE("spawn_executor executes a task", "[spawn]") {
    test_can_execute_a_task(concore::spawn_executor{});
}

TEST_CASE("spawn_executor executes all tasks", "[spawn]") {
    test_can_execute_multiple_tasks(concore::spawn_executor{});
}

void task_fun(task_countdown& tc, int n) {
    CONCORE_PROFILING_FUNCTION();
    // Each task spawns the new one
    if (n > 0)
        concore::spawn([&tc, n]() { task_fun(tc, n - 1); });
    tc.task_finished();
}

TEST_CASE("spawning tasks from existing tasks", "[spawn]") {
    constexpr int num_tasks = 20;
    task_countdown tc{num_tasks};
    concore::spawn([&tc]() { task_fun(tc, num_tasks - 1); });

    // Wait for all the tasks to complete
    REQUIRE(tc.wait_for_all());
}

TEST_CASE("spawned tasks can be executed by multiple workers", "[spawn]") {
    CONCORE_PROFILING_FUNCTION();

    // This only works if we have multiple cores
    if (std::thread::hardware_concurrency() < 4)
        return;

    // We are not guaranteed to find an out-of-order execution pattern; so try multiple times.
    constexpr int max_runs = 100;
    for (int k = 0; k < max_runs; k++) {
        constexpr int num_tasks = 20;

        std::array<int, num_tasks> starts{};
        std::array<int, num_tasks> ends{};
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

TEST_CASE("a lot of spawning of small tasks works", "[spawn]") {
    CONCORE_PROFILING_FUNCTION();

    constexpr int num_tasks = 1000;
    std::atomic<int> count{0};

    // Spawn 1+num_tasks and wait for them to complete
    concore::spawn_and_wait([&count]() {
        for (int i = 0; i < num_tasks; i++) {
            concore::spawn([&count]() { count++; });
        }
    });

    // Check that we've executed the right amount of tasks
    REQUIRE(count.load() == num_tasks);
}

TEST_CASE("wait does work on the calling thread", "[spawn]") {
    CONCORE_PROFILING_FUNCTION();

    // For this test we need exactly 1 worker thread
    concore::shutdown();
    concore::init_data config;
    config.num_workers_ = 1;
    concore::init(config);

    std::atomic<bool> can_finish{false};
    std::atomic<int> count{0};

    auto grp = concore::task_group::create();

    // Enqueue a task that will wait run for as long as the can_finish is not set
    concore::spawn(
            [&] {
                while (!can_finish.load())
                    std::this_thread::sleep_for(1ms);
                count++;
            },
            grp);

    // Enqueue the second task that will unblock the first one
    concore::spawn(
            [&] {
                can_finish = true;
                count++;
            },
            grp);

    // At this point, no task should be finished
    std::this_thread::sleep_for(3ms);
    REQUIRE(count.load() == 0);

    // Wait for the group to complete.
    // As this thread joins the workers, the two tasks will be executed
    concore::wait(grp);
    REQUIRE(count.load() == 2);

    // Reset the number of workers
    concore::shutdown();
}
