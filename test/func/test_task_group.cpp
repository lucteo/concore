#include <catch2/catch.hpp>
#include <concore/task_group.hpp>
#include <concore/spawn.hpp>
#include <concore/global_executor.hpp>
#include <concore/serializer.hpp>
#include <concore/n_serializer.hpp>
#include <concore/rw_serializer.hpp>
#include <concore/profiling.hpp>

#include <thread>
#include <atomic>
#include <chrono>

using namespace std::chrono_literals;

TEST_CASE("can create a task_group", "[task_group]") {
    SECTION("default ctor") {
        concore::task_group grp;
        REQUIRE_FALSE(static_cast<bool>(grp));
    }
    SECTION("create method") {
        auto grp = concore::task_group::create();
        REQUIRE(static_cast<bool>(grp));
        REQUIRE_FALSE(grp.is_active());
    }
    SECTION("create method with parent") {
        auto grp1 = concore::task_group::create();
        auto grp = concore::task_group::create(grp1);
        REQUIRE(static_cast<bool>(grp1));
        REQUIRE(static_cast<bool>(grp));

        REQUIRE_FALSE(grp1.is_active());
        REQUIRE_FALSE(grp.is_active());
    }
}

TEST_CASE("can cancel a task_group", "[task_group]") {
    auto grp = concore::task_group::create();
    REQUIRE_FALSE(grp.is_cancelled());
    grp.cancel();
    REQUIRE(grp.is_cancelled());
    grp.clear_cancel();
    REQUIRE_FALSE(grp.is_cancelled());
    REQUIRE_FALSE(grp.is_active());
}

TEST_CASE("task_group can cancel tasks", "[task_group]") {
    auto grp = concore::task_group::create();
    auto ftor = []() { FAIL("task is executed, and it shouldn't be"); };
    auto t = concore::task(ftor, grp);

    // cancel the task_group object
    grp.cancel();

    // now try to execute the task
    concore::spawn(std::move(t));

    std::this_thread::sleep_for(3ms);
    // the task should not be executed
}

TEST_CASE("task_group cancellation is recursive", "[task_group]") {
    auto grp1 = concore::task_group::create();
    auto grp2 = concore::task_group::create(grp1);
    auto ftor = []() { FAIL("task is executed, and it shouldn't be"); };
    auto t = concore::task(ftor, grp2);

    // cancel the parent task_group object
    grp1.cancel();

    // now try to execute the task
    concore::spawn(std::move(t));

    std::this_thread::sleep_for(3ms);
    // the task should not be executed
}

TEST_CASE("one can check cancellation from within a task", "[task_group]") {
    auto grp = concore::task_group::create();
    auto long_task_ftor = []() {
        int counter = 0;
        while (!concore::task_group::is_current_task_cancelled()) {
            std::this_thread::sleep_for(1ms);
            if (counter++ > 1000)
                FAIL("task was not properly canceled in time");
        }
        REQUIRE(concore::task_group::is_current_task_cancelled());
    };
    auto long_task = concore::task(long_task_ftor, grp);
    concore::spawn(std::move(long_task));

    // Wait a bit for the task to start, and cancel the task group
    std::this_thread::sleep_for(3ms);
    grp.cancel();

    // wait until the task finishes
    while (grp.is_active())
        std::this_thread::sleep_for(1ms);

    SUCCEED("task properly canceled");
}

TEST_CASE("task_group can be used to wait for tasks", "[task_group]") {
    auto grp = concore::task_group::create();
    std::atomic<bool> executed{false};
    auto ftor = [&]() {
        std::this_thread::sleep_for(3ms);
        executed.store(true);
    };
    auto t = concore::task(ftor, grp);

    REQUIRE(grp.is_active());

    // spawn the task and wait for it
    concore::spawn(std::move(t));

    while (grp.is_active())
        std::this_thread::sleep_for(500us);

    REQUIRE(executed.load());
    REQUIRE_FALSE(grp.is_active());
}

TEST_CASE("task_group is called on exception", "[task_group]") {
    auto grp = concore::task_group::create();
    std::atomic<bool> exception_caught{false};
    auto except_fun = [&](std::exception_ptr) {
        exception_caught.store(true);
        SUCCEED("yes, we've caught the exception");
    };
    grp.set_exception_handler(except_fun);

    auto ftor = []() { throw std::logic_error("some exception"); };
    auto t = concore::task(ftor, grp);

    // spawn the task and wait for it
    concore::spawn(std::move(t));
    while (grp.is_active())
        std::this_thread::sleep_for(500us);

    REQUIRE(exception_caught.load());
}

TEST_CASE("task_group is inherited on spawn", "[task_group]") {
    auto grp = concore::task_group::create();
    auto long_task_ftor = []() {
        int counter = 0;
        while (!concore::task_group::is_current_task_cancelled()) {
            std::this_thread::sleep_for(1ms);
            if (counter++ > 1000)
                FAIL("task was not properly canceled in time");
        }
        REQUIRE(concore::task_group::is_current_task_cancelled());
    };
    auto parent_task_ftor = [&]() {
        // spawn once
        concore::spawn(long_task_ftor);
        // spawn several more tasks
        concore::spawn({long_task_ftor, long_task_ftor, long_task_ftor});
    };
    concore::spawn(concore::task(parent_task_ftor, grp));

    // Wait a bit for the tasks to start, and cancel the task group
    std::this_thread::sleep_for(3ms);
    grp.cancel();

    // wait until the tasks finish
    while (grp.is_active())
        std::this_thread::sleep_for(1ms);

    SUCCEED("tasks properly canceled");
}

void test_task_group_and_serializers(concore::any_executor executor) {
    auto grp = concore::task_group::create();
    std::atomic<int> num_cancelled{0};
    auto ftor = [&num_cancelled]() {
        CONCORE_PROFILING_SCOPE_N("ftor");
        int counter = 0;
        while (!concore::task_group::is_current_task_cancelled()) {
            std::this_thread::sleep_for(1ms);
            if (counter++ > 1000) {
                FAIL("task was not properly canceled in time");
                return;
            }
        }
        num_cancelled++;
    };
    // Start the tasks
    for (int i = 0; i < 10; i++)
        executor.execute(concore::task(ftor, grp));

    // Add another task at the end; this time outside of the task_group
    std::atomic<bool> reached_end_task{false};
    auto grpEnd = concore::task_group::create();
    executor.execute(concore::task{[&]() { reached_end_task = true; }, grpEnd});

    // Wait a bit for the tasks to start, and cancel the task group
    std::this_thread::sleep_for(3ms);
    grp.cancel();

    // wait until the tasks finish
    while (grp.is_active())
        std::this_thread::sleep_for(1ms);

    // Now, ensure that we execute the last task
    concore::wait(grpEnd);
    REQUIRE(num_cancelled.load() >= 1);
    REQUIRE(reached_end_task.load());

    SUCCEED("tasks properly canceled");
}

TEST_CASE("task_group is inherited when using other concore task executors", "[task_group]") {
    SECTION("serializer") {
        test_task_group_and_serializers(concore::serializer(concore::global_executor{}));
    }
    SECTION("n_serializer") {
        test_task_group_and_serializers(concore::n_serializer(4, concore::global_executor{}));
    }
    SECTION("rw_serializer") {
        concore::rw_serializer ser(concore::global_executor{});
        test_task_group_and_serializers(ser.reader());
        test_task_group_and_serializers(ser.writer());
    }
    SECTION("sanity check: also test spawning with this method") {
        test_task_group_and_serializers(concore::spawn_executor{});
        test_task_group_and_serializers(concore::spawn_continuation_executor{});
    }
}

TEST_CASE("task_group::is_active doesn't count the sub-groups", "[task_group]") {
    auto grp = concore::task_group::create();
    auto grp1 = concore::task_group::create(grp);
    auto grp2 = concore::task_group::create(grp);
    auto grp3 = concore::task_group::create(grp);
    REQUIRE_FALSE(grp.is_active());
    REQUIRE_FALSE(grp1.is_active());
    REQUIRE_FALSE(grp2.is_active());
    REQUIRE_FALSE(grp3.is_active());
}

TEST_CASE("task_group::is_active doesn't return true for multiple copies of the group",
        "[task_group]") {
    auto grp = concore::task_group::create();
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    auto grp1 = grp;
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    auto grp2 = grp;
    REQUIRE_FALSE(grp.is_active());
    REQUIRE_FALSE(grp1.is_active());
    REQUIRE_FALSE(grp2.is_active());
}

TEST_CASE("task_group::is_active also counts the tasks from sub-groups", "[task_group]") {
    auto grpMain = concore::task_group::create();
    auto grpSub = concore::task_group::create(grpMain);
    REQUIRE_FALSE(grpMain.is_active());
    REQUIRE_FALSE(grpSub.is_active());

    auto t1 = concore::task([]() {}, grpSub);
    REQUIRE(grpSub.is_active());
    REQUIRE(grpMain.is_active());
}

TEST_CASE("an empty group cannot be active", "[task_group]") {
    concore::task_group grp;
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    concore::task_group grp1 = grp;
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    concore::task_group grp2 = grp;
    REQUIRE_FALSE(grp.is_active());
    REQUIRE_FALSE(grp1.is_active());
    REQUIRE_FALSE(grp2.is_active());
}

TEST_CASE("higher level except handler is called, if the child doesn't have one", "[task_group]") {
    auto grpTop = concore::task_group::create();
    auto grpChild1 = concore::task_group::create(grpTop);
    auto grpChild2 = concore::task_group::create(grpTop);
    int cnt_ex_top = 0;
    int cnt_ex_child1 = 0;
    grpTop.set_exception_handler([&](std::exception_ptr) { cnt_ex_top++; });
    grpChild1.set_exception_handler([&](std::exception_ptr) { cnt_ex_child1++; });

    // Run a task in each group
    concore::spawn(concore::task([] { throw 1; }, grpTop));
    concore::wait(grpTop);
    REQUIRE(cnt_ex_top == 1);
    REQUIRE(cnt_ex_child1 == 0);
    concore::spawn(concore::task([] { throw 1; }, grpChild1));
    concore::wait(grpTop);
    REQUIRE(cnt_ex_top == 1);
    REQUIRE(cnt_ex_child1 == 1);
    concore::spawn(concore::task([] { throw 1; }, grpChild2));
    concore::wait(grpTop);
    REQUIRE(cnt_ex_top == 2);
    REQUIRE(cnt_ex_child1 == 1);
}

TEST_CASE("task notifies task_group when it's created with a group", "[task_group]") {
    // Create a group; it should be inactive
    auto grp = concore::task_group::create();
    CHECK_FALSE(grp.is_active());

    {
        // Create a task with our group
        concore::task t{[] {}, grp};

        // Now the group should be active
        CHECK(grp.is_active());
    }

    // After the task is destroyed the group is inactive again
    CHECK_FALSE(grp.is_active());
}
TEST_CASE("task notifies task_group when setting a new group", "[task_group]") {
    // Create a group; it should be inactive
    auto grp = concore::task_group::create();
    CHECK_FALSE(grp.is_active());

    {
        // Create a task with no group; our group should still be inactive
        concore::task t{[] {}, {}};
        CHECK_FALSE(grp.is_active());

        // Set the group to the task
        t.set_task_group(grp);

        // Now the group should be active
        CHECK(grp.is_active());
    }

    // After the task is destroyed the group is inactive again
    CHECK_FALSE(grp.is_active());
}

// Functor that checks that the group is active at the destruction point
// This can be used both as a task function object, or continuation object
struct group_checker_ftor {
    concore::task_group grp_;
    bool* should_check_;

    group_checker_ftor(const group_checker_ftor&) = default;
    group_checker_ftor(group_checker_ftor&&) = default;
    group_checker_ftor& operator=(const group_checker_ftor&) = default;
    group_checker_ftor& operator=(group_checker_ftor&&) = default;

    ~group_checker_ftor() {
        if (*should_check_) {
            CHECK(grp_.is_active());
        }
    }

    void operator()() {
        if (*should_check_) {
            CHECK(grp_.is_active());
        }
    }
    void operator()(std::exception_ptr) {
        if (*should_check_) {
            CHECK(grp_.is_active());
        }
    }
};

TEST_CASE("task_group is active when the task function & continuation are destroyed",
        "[task_group]") {
    // Create a group; it should be inactive
    auto grp = concore::task_group::create();
    CHECK_FALSE(grp.is_active());

    bool should_check{false};

    {
        // Create a task with no group; our group should still be inactive
        concore::task t{group_checker_ftor{grp, &should_check}, grp,
                group_checker_ftor{grp, &should_check}};

        // After both functors are created, and the task joined the group turn on the checking for
        // the group active
        CHECK(grp.is_active());
        should_check = true;
    }
    // When destroying the task, the group must remain active until both ftors are destroyed

    // After the task is destroyed the group is inactive again
    CHECK_FALSE(grp.is_active());
}
