#ifdef CONCORE_USE_TBB

#include <catch2/catch.hpp>
#include <concore/tbb_executor.hpp>
#include "test_common/common_executor_tests.hpp"

using namespace std::chrono_literals;

TEST_CASE("tbb_executor is copyable") {
    CONCORE_PROFILING_FUNCTION();

    auto e1 = concore::tbb_executor;
    auto e2 = concore::tbb_executor;
    e2 = e1;
}

TEST_CASE("tbb_executor executes a task") { test_can_execute_a_task(concore::tbb_executor); }

TEST_CASE("tbb_executor executes all tasks") {
    test_can_execute_multiple_tasks(concore::tbb_executor);
}

TEST_CASE("tbb_executor's task completion is out-of-order") {
    test_tasks_arrive_out_of_order(concore::tbb_executor);
}

TEST_CASE("tbb_executor runs tasks in parallel") {
    test_tasks_do_run_in_parallel(concore::tbb_executor);
}

#endif
