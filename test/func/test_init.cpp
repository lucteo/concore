#include <catch2/catch.hpp>
#include <concore/init.hpp>
#include <concore/global_executor.hpp>
#include <concore/profiling.hpp>

#include "test_common/common_executor_tests.hpp"
#include "test_common/task_utils.hpp"

using namespace std::chrono_literals;

TEST_CASE("can init and shutdown library", "[init]") {
    concore::shutdown();
    REQUIRE_FALSE(concore::is_initialized());
    concore::init();
    REQUIRE(concore::is_initialized());
    concore::shutdown();
    REQUIRE_FALSE(concore::is_initialized());
}

TEST_CASE("can init multiple times", "[init]") {
    concore::shutdown();
    REQUIRE_FALSE(concore::is_initialized());
    concore::init();
    REQUIRE(concore::is_initialized());
    concore::shutdown();
    REQUIRE_FALSE(concore::is_initialized());
    concore::init();
    REQUIRE(concore::is_initialized());
    concore::shutdown();
}

TEST_CASE("can execute tasks if initialized multiple times", "[init]") {
    concore::shutdown();
    REQUIRE_FALSE(concore::is_initialized());
    concore::init();
    REQUIRE(concore::is_initialized());
    concore::shutdown();
    REQUIRE_FALSE(concore::is_initialized());
    concore::init();
    REQUIRE(concore::is_initialized());
    test_can_execute_a_task(concore::global_executor);
    concore::shutdown();
}

TEST_CASE("limiting the number of threads is visible", "[init]") {
    concore::shutdown();

    for (int i = 1; i < 10; i++) {
        concore::init_data config;
        config.num_workers_ = i;
        concore::init(config);

        auto start = std::chrono::steady_clock::now();

        REQUIRE(enqueue_and_wait(
                concore::global_executor, []() { std::this_thread::sleep_for(3ms); }));

        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> diff = end - start;
        double expected_time = 3.0 / double(i) * 10.0 / 1000.0;

        REQUIRE(diff.count() >= expected_time);

        concore::shutdown();
    }
}

TEST_CASE("worker start fun is called", "[init]") {
    concore::shutdown();

    for (int i = 1; i < 10; i++) {
        std::atomic<int> init_count = 0;

        concore::init_data config;
        config.num_workers_ = i;
        config.worker_start_fun_ = [&]() { init_count++; };
        concore::init(config);

        // Wait a but for the threads to start, and check that we started the indicated number of
        // threads.
        for (int j = 0; j < 10; j++) {
            if (init_count.load() == i)
                break;
            std::this_thread::sleep_for(1ms);
        }
        REQUIRE(init_count.load() == i);

        concore::shutdown();
    }
}

TEST_CASE("init twice (manually) throws", "[init]") {
    concore::shutdown();
    concore::init();

    REQUIRE_THROWS_AS(concore::init(), concore::already_initialized);
}
TEST_CASE("init twice (first time automatic) throws", "[init]") {
    concore::shutdown();
    // Enqueueing a task here initializes the library
    test_can_execute_a_task(concore::global_executor);

    REQUIRE_THROWS_AS(concore::init(), concore::already_initialized);
}
