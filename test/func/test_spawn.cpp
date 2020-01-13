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
    if ( n > 0 )
        concore::spawn([&tc, n]() { task_fun(tc, n-1); });
    tc.task_finished();
}

TEST_CASE("spawning tasks from existing tasks") {
    constexpr int num_tasks = 20;
    task_countdown tc{num_tasks};
    concore::spawn([&tc, num_tasks]() { task_fun(tc, num_tasks-1); });

    // Wait for all the tasks to complete
    REQUIRE(tc.wait_for_all());
}
