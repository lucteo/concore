#include <concore/dispatch_executor.hpp>

#if CONCORE_USE_LIBDISPATCH
#include <catch2/catch.hpp>
#include "test_common/common_executor_tests.hpp"

using namespace std::chrono_literals;

TEST_CASE("dispatch_executor is copyable") {
    CONCORE_PROFILING_FUNCTION();

    auto e1 = concore::dispatch_executor{};
    auto e2 = concore::dispatch_executor{};
    e2 = e1;
}

TEST_CASE("dispatch_executor executes a task") {
    test_can_execute_a_task(concore::dispatch_executor{});
}

TEST_CASE("dispatch_executor executes all tasks") {
    test_can_execute_multiple_tasks(concore::dispatch_executor{});
}

TEST_CASE("dispatch_executor runs tasks in parallel") {
    test_tasks_do_run_in_parallel(concore::dispatch_executor{});
}

#endif
