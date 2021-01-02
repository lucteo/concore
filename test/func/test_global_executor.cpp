#include <catch2/catch.hpp>
#include <concore/global_executor.hpp>
#include <concore/executor_type.hpp>

#include "test_common/common_executor_tests.hpp"
#include "test_common/task_countdown.hpp"

#include <array>

using namespace std::chrono_literals;

TEST_CASE("global_executor is copyable") {
    auto e1 = concore::global_executor{};
    auto e2 = concore::global_executor{};
    // cppcheck-suppress redundantInitialization
    // cppcheck-suppress unreadVariable
    e2 = e1;
}

TEST_CASE("global_executor executes a task") {
    test_can_execute_a_task(concore::global_executor{});
}

TEST_CASE("global_executor executes all tasks") {
    test_can_execute_multiple_tasks(concore::global_executor{});
}

TEST_CASE("global_executor runs tasks in parallel") {
    test_tasks_do_run_in_parallel(concore::global_executor{});
}

TEST_CASE("global_executor executes tasks according to their prio") {
    // Run this multiple times, as execution order can be tricky
    constexpr int num_runs = 10;
    for (int k = 0; k < num_runs; k++) {
        // The executors for all the priorities that we have
        constexpr int num_prios = 5;
        std::array<concore::executor_t, num_prios> executors = {
                concore::global_executor(concore::global_executor::prio_background),
                concore::global_executor(concore::global_executor::prio_low),
                concore::global_executor(concore::global_executor::prio_normal),
                concore::global_executor(concore::global_executor::prio_high),
                concore::global_executor(concore::global_executor::prio_critical)};

        constexpr int num_tasks_per_prio = 30;
        constexpr int num_tasks = num_prios * num_tasks_per_prio;
        task_countdown tc{num_tasks};

        // We store the priorities of each task; the order will be consistent with the task
        // execution order.
        std::array<int, num_tasks> task_prios{};
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
