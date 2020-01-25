#include <catch2/catch.hpp>
#include <concore/task_control.hpp>
#include <concore/spawn.hpp>

#include <thread>
#include <atomic>
#include <chrono>

using namespace std::chrono_literals;

TEST_CASE("can create a task_control", "[task_control]") {
    SECTION("default ctor") {
        concore::task_control tc;
        REQUIRE_FALSE(static_cast<bool>(tc));
    }
    SECTION("create method") {
        auto tc = concore::task_control::create();
        REQUIRE(static_cast<bool>(tc));
        REQUIRE_FALSE(tc.is_active());
    }
    SECTION("create method with parent") {
        auto tc1 = concore::task_control::create();
        auto tc = concore::task_control::create(tc1);
        REQUIRE(static_cast<bool>(tc1));
        REQUIRE(static_cast<bool>(tc));

        REQUIRE(tc1.is_active()); // has one children
        REQUIRE_FALSE(tc.is_active());
    }
}

TEST_CASE("can cancel a task_control", "[task_control]") {
    auto tc = concore::task_control::create();
    REQUIRE_FALSE(tc.is_cancelled());
    tc.cancel();
    REQUIRE(tc.is_cancelled());
    tc.clear_cancel();
    REQUIRE_FALSE(tc.is_cancelled());
    REQUIRE_FALSE(tc.is_active());
}

TEST_CASE("task_control can cancel tasks", "[task_control]") {
    auto tc = concore::task_control::create();
    auto ftor = []() { FAIL("task is executed, and it shouldn't be"); };
    auto t = concore::task(ftor, tc);

    // cancel the task_control object
    tc.cancel();

    // now try to execute the task
    concore::spawn(std::move(t));

    std::this_thread::sleep_for(3ms);
    // the task should not be executed
}

TEST_CASE("task_control cancellation is recursive", "[task_control]") {
    auto tc1 = concore::task_control::create();
    auto tc2 = concore::task_control::create(tc1);
    auto ftor = []() { FAIL("task is executed, and it shouldn't be"); };
    auto t = concore::task(ftor, tc2);

    // cancel the parent task_control object
    tc1.cancel();

    // now try to execute the task
    concore::spawn(std::move(t));

    std::this_thread::sleep_for(3ms);
    // the task should not be executed
}

TEST_CASE("one can check cancellation from within a task", "[task_control]") {
    auto tc = concore::task_control::create();
    auto long_task_ftor = []() {
        int counter = 0;
        while (!concore::task_control::is_current_task_cancelled()) {
            std::this_thread::sleep_for(1ms);
            if (counter++ > 1000)
                FAIL("task was not properly canceled in time");
        }
        REQUIRE(concore::task_control::is_current_task_cancelled());
    };
    auto long_task = concore::task(long_task_ftor, tc);
    concore::spawn(std::move(long_task));

    // Wait a bit for the task to start, and cancel the task control
    std::this_thread::sleep_for(3ms);
    tc.cancel();

    // wait until the task finishes
    while (tc.is_active())
        std::this_thread::sleep_for(1ms);

    SUCCEED("task properly canceled");
}

TEST_CASE("task_control can be used to wait for tasks", "[task_control]") {
    auto tc = concore::task_control::create();
    std::atomic<bool> executed{false};
    auto ftor = [&]() {
        std::this_thread::sleep_for(3ms);
        executed.store(true);
    };
    auto t = concore::task(ftor, tc);

    REQUIRE(tc.is_active());

    // spawn the task and wait for it
    concore::spawn(std::move(t));

    while (tc.is_active())
        std::this_thread::sleep_for(500us);

    REQUIRE(executed.load());
    REQUIRE_FALSE(tc.is_active());
}

TEST_CASE("task_control is called on exception", "[task_control]") {
    auto tc = concore::task_control::create();
    std::atomic<bool> exception_caught{false};
    auto except_fun = [&](std::exception_ptr) {
        exception_caught.store(true);
        SUCCEED("yes, we've caught the exception");
    };
    tc.set_exception_handler(except_fun);

    auto ftor = []() { throw std::logic_error("some exception"); };
    auto t = concore::task(ftor, tc);

    // spawn the task and wait for it
    concore::spawn(std::move(t));
    while (tc.is_active())
        std::this_thread::sleep_for(500us);

    REQUIRE(exception_caught.load());
}

TEST_CASE("task_control is inherited on spawn", "[task_control]") {
    auto tc = concore::task_control::create();
    auto long_task_ftor = []() {
        int counter = 0;
        while (!concore::task_control::is_current_task_cancelled()) {
            std::this_thread::sleep_for(1ms);
            if (counter++ > 1000)
                FAIL("task was not properly canceled in time");
        }
        REQUIRE(concore::task_control::is_current_task_cancelled());
    };
    auto parent_task_ftor = [&]() {
        // spawn once
        concore::spawn(long_task_ftor);
        // spawn several more tasks
        concore::spawn({long_task_ftor, long_task_ftor, long_task_ftor});
    };
    concore::spawn(concore::task(parent_task_ftor, tc));

    // Wait a bit for the tasks to start, and cancel the task control
    std::this_thread::sleep_for(3ms);
    tc.cancel();

    // wait until the tasks finish
    while (tc.is_active())
        std::this_thread::sleep_for(1ms);

    SUCCEED("tasks properly canceled");
}

