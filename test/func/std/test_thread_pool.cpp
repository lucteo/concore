#include <catch2/catch.hpp>
#include <concore/std/thread_pool.hpp>
#include "test_common/task_utils.hpp"

#include <thread>
#include <chrono>

using namespace concore::std_execution;
using namespace std::chrono_literals;

TEST_CASE("Can create a static_thread_pool", "[execution]") {

    static_thread_pool my_pool{4};
    my_pool.executor(); // discard the result
    // destructor is called now
}

TEST_CASE("A static_thread_pool can execute work", "[execution]") {
    bool executed = false;
    {
        static_thread_pool my_pool{4};
        auto ex = my_pool.executor();
        ex.execute([&] { executed = true; });
        // wait for the task to be executed
        auto grp = detail::get_associated_group(my_pool);
        REQUIRE(bounded_wait(grp));
    }
    REQUIRE(executed);
}

TEST_CASE("A static_thread_pool can bulk_execute work", "[execution]") {
    constexpr size_t num_items = 100;
    std::array<bool, num_items> executed{};
    executed.fill(false);
    {
        static_thread_pool my_pool{4};
        auto ex = my_pool.executor();
        ex.bulk_execute([&](size_t i) { executed[i] = true; }, num_items);
        // wait for the task to be executed
        auto grp = detail::get_associated_group(my_pool);
        REQUIRE(bounded_wait(grp));
    }
    for (size_t i = 0; i < num_items; i++)
        REQUIRE(executed[i]);
}

TEST_CASE("static_thread_pool executor's running_in_this_thread returns false if called from a "
          "different thread",
        "[execution]") {}
TEST_CASE("static_thread_pool executor's running_in_this_thread returns true if called from inside "
          "a running task",
        "[execution]") {}
TEST_CASE("static_thread_pool cannot execute more than maximum concurrency tasks in parallel",
        "[execution]") {}
TEST_CASE("static_thread_pool::attach will make the calling thread join the pool", "[execution]") {}
TEST_CASE("static_thread_pool::attach will increase the number of threads in the pool",
        "[execution]") {}
TEST_CASE("static_thread_pool does not take new tasks after stop()", "[execution]") {}
TEST_CASE("static_thread_pool does not take new tasks after wait()", "[execution]") {}
TEST_CASE("static_thread_pool waits for all the tasks to be done when calling wait()",
        "[execution]") {}
