#include <catch2/catch.hpp>
#include <concore/finish_task.hpp>
#include <concore/delegating_executor.hpp>
#include "rapidcheck_utils.hpp"

#include <thread>
#include <chrono>

using namespace std::chrono_literals;

using concore::task;

TEST_CASE("finish_task basic usage", "[finish_task]") {
    std::atomic<bool> is_done{false};
    std::atomic<bool> work1_done{false};
    std::atomic<bool> work2_done{false};
    std::atomic<bool> work3_done{false};

    concore::finish_task done_task([&] { is_done = true; });
    // Spawn 3 tasks
    auto cont = done_task.get_continuation(3); // this continuation will be used 3 times
    concore::spawn(concore::task{[&] { work1_done = true; }, {}, cont});
    concore::spawn(concore::task{[&] { work2_done = true; }, {}, cont});
    concore::spawn(concore::task{[&] { work3_done = true; }, {}, cont});

    // Poor-man wait for all to be complete
    while (!is_done.load())
        std::this_thread::sleep_for(1ms);

    CHECK(work1_done.load());
    CHECK(work2_done.load());
    CHECK(work3_done.load());
    CHECK(is_done.load());
}

TEST_CASE("finish_task is not triggered before the last task", "[finish_task]") {
    std::atomic<bool> is_done{false};
    std::atomic<bool> work1_done{false};
    std::atomic<bool> work2_done{false};
    std::atomic<bool> work3_done{false};

    concore::finish_task done_task([&] { is_done = true; });
    // Spawn 3 tasks
    auto cont = done_task.get_continuation(3); // this continuation will be used 3 times
    concore::spawn(concore::task{[&] { work1_done = true; }, {}, cont});
    while (!work1_done.load())
        std::this_thread::sleep_for(200us);
    CHECK_FALSE(is_done.load());
    concore::spawn(concore::task{[&] { work2_done = true; }, {}, cont});
    while (!work2_done.load())
        std::this_thread::sleep_for(200us);
    CHECK_FALSE(is_done.load());
    concore::spawn(concore::task{[&] { work3_done = true; }, {}, cont});

    // Poor-man wait for all to be complete
    while (!is_done.load())
        std::this_thread::sleep_for(1ms);

    CHECK(work1_done.load());
    CHECK(work2_done.load());
    CHECK(work3_done.load());
    CHECK(is_done.load());
}

TEST_CASE("finish_task basic usage (old)", "[finish_task]") {
    std::atomic<bool> is_done{false};
    std::atomic<bool> work1_done{false};
    std::atomic<bool> work2_done{false};
    std::atomic<bool> work3_done{false};

    concore::finish_task done_task([&] { is_done = true; }, 3);
    // Spawn 3 tasks
    auto event = done_task.event();
    concore::spawn([&, event] {
        work1_done = true;
        event.notify_done();
    });
    concore::spawn([&, event] {
        work2_done = true;
        event.notify_done();
    });
    concore::spawn([&, event] {
        work3_done = true;
        event.notify_done();
    });

    // Poor-man wait for all to be complete
    while (!is_done.load())
        std::this_thread::sleep_for(1ms);

    CHECK(work1_done.load());
    CHECK(work2_done.load());
    CHECK(work3_done.load());
    CHECK(is_done.load());
}

TEST_CASE("finish_wait basic usage", "[finish_task]") {
    std::atomic<bool> work1_done{false};
    std::atomic<bool> work2_done{false};
    std::atomic<bool> work3_done{false};

    concore::finish_wait done;
    // Spawn 3 tasks
    concore::spawn(concore::task{[&] { work1_done = true; }, {}, done.get_continuation()});
    concore::spawn(concore::task{[&] { work2_done = true; }, {}, done.get_continuation()});
    concore::spawn(concore::task{[&] { work3_done = true; }, {}, done.get_continuation()});

    done.wait();

    CHECK(work1_done.load());
    CHECK(work2_done.load());
    CHECK(work3_done.load());
}

TEST_CASE("finish_wait with tasks executing before another get_continuation", "[finish_task]") {
    std::atomic<bool> work1_done{false};
    std::atomic<bool> work2_done{false};

    concore::finish_wait done;

    // Spawn the first task and wait for it to complete
    concore::spawn(concore::task{[&] { work1_done = true; }, {}, done.get_continuation()});
    while (!work1_done.load())
        std::this_thread::sleep_for(200us);

    // Span the second task
    // This is a longer task, given a chance for wait() to finish early
    concore::spawn(concore::task{[&] {
                                     std::this_thread::sleep_for(1ms);
                                     work2_done = true;
                                 },
            {}, done.get_continuation()});

    // Wait for both tasks to be done
    done.wait();

    // Ensure that we've waited for the results of both tasks
    CHECK(work1_done.load());
    CHECK(work2_done.load());
}

TEST_CASE("finish_wait with multiple wait calls", "[finish_task]") {
    concore::finish_wait done;

    // Get a continuation and immediately trigger it
    concore::spawn(concore::task{[] {}, {}, done.get_continuation()});

    // Wait once
    done.wait();

    // Second wait unblocks immediately, even if we have a continuation open
    auto c = done.get_continuation();
    done.wait();
}

TEST_CASE("finish_wait basic usage (old)", "[finish_task]") {
    std::atomic<bool> work1_done{false};
    std::atomic<bool> work2_done{false};
    std::atomic<bool> work3_done{false};

    concore::finish_wait done(3);
    // Spawn 3 tasks
    auto event = done.event();
    concore::spawn([&, event] {
        std::this_thread::sleep_for(1ms);
        work1_done = true;
        event.notify_done();
    });
    concore::spawn([&, event] {
        std::this_thread::sleep_for(2ms);
        work2_done = true;
        event.notify_done();
    });
    concore::spawn([&, event] {
        std::this_thread::sleep_for(3ms);
        work3_done = true;
        event.notify_done();
    });

    done.wait();

    CHECK(work1_done.load());
    CHECK(work2_done.load());
    CHECK(work3_done.load());
}

TEST_CASE("finish_task with an arbitrary count", "[finish_task]") {
    PROPERTY([]() {
        int count = *rc::gen::inRange(1, 100);
        int num_tasks = count;
        bool should_have_more_tasks = *rc::gen::inRange(0, 5) == 0;
        if (should_have_more_tasks)
            num_tasks = *rc::gen::inRange(count, count * 2);

        concore::task_group wait_grp = concore::task_group::create();

        // Create a finish_task
        std::atomic<bool> is_done{false};
        concore::finish_task done_task(task{[&] { is_done = true; }, wait_grp}, count);
        auto event = done_task.event();

        // Spawn some tasks that will notify the finish task
        // We may have more tasks than needed
        std::atomic<int> executed_count{0};
        for (int i = 0; i < num_tasks; i++) {
            auto task_body = [&] {
                std::this_thread::sleep_for(10us);
                executed_count++;
                event.notify_done();
            };
            concore::spawn(task{task_body, wait_grp});
        }

        // Poor-man wait for all to be complete
        for (int i = 0; i < 1000 && !is_done.load(); i++)
            std::this_thread::sleep_for(10us);

        RC_ASSERT(is_done.load());
        RC_ASSERT(executed_count.load() <= num_tasks);

        // Wait for all spawned tasks to complete
        concore::wait(wait_grp);

        RC_ASSERT(is_done.load());
        RC_ASSERT(executed_count.load() == num_tasks);
    });
}

TEST_CASE("finish_wait with an arbitrary count", "[finish_task]") {
    PROPERTY([]() {
        int count = *rc::gen::inRange(1, 100);
        int num_tasks = count;
        bool should_have_more_tasks = *rc::gen::inRange(0, 5) == 0;
        if (should_have_more_tasks)
            num_tasks = *rc::gen::inRange(count, count * 2);

        concore::task_group wait_grp = concore::task_group::create();

        // Create a finish_task
        concore::finish_wait done{1};
        auto event = done.event();

        // Spawn some tasks that will notify the finish task
        // We may have more tasks than needed
        std::atomic<int> executed_count{0};
        for (int i = 0; i < num_tasks; i++) {
            auto task_body = [&] {
                std::this_thread::sleep_for(10us);
                executed_count++;
                event.notify_done();
            };
            concore::spawn(task{task_body, wait_grp});
        }

        // Wait for 'count' tasks to complete
        done.wait();

        RC_ASSERT(executed_count.load() <= num_tasks);

        // Wait for all spawned tasks to complete
        concore::wait(wait_grp);

        RC_ASSERT(executed_count.load() == num_tasks);
    });
}

TEST_CASE("multiple calls to notify_done() in a row", "[finish_task]") {
    int count = 10;

    std::atomic<int> done_cnt{0};
    concore::finish_task done_task([&] { done_cnt++; }, concore::inline_executor{}, count);
    auto event = done_task.event();

    CHECK(done_cnt.load() == 0);

    // Call notify_done() for the right amount of times
    for (int i = 0; i < count; i++)
        event.notify_done();

    CHECK(done_cnt.load() == 1);

    // Call it several times more
    for (int i = 0; i < count; i++)
        event.notify_done();

    CHECK(done_cnt.load() == 1);
}

TEST_CASE("multiple wait() calls on finish_wait do not wait", "[finish_task]") {
    std::atomic<bool> work1_done{false};
    std::atomic<bool> work2_done{false};
    std::atomic<bool> work3_done{false};

    concore::finish_wait done;
    // Spawn 3 tasks
    auto f1 = [&] {
        std::this_thread::sleep_for(1ms);
        work1_done = true;
    };
    auto f2 = [&] {
        std::this_thread::sleep_for(2ms);
        work2_done = true;
    };
    auto f3 = [&] {
        std::this_thread::sleep_for(3ms);
        work3_done = true;
    };
    concore::spawn(concore::task{std::move(f1), {}, done.get_continuation()});
    concore::spawn(concore::task{std::move(f2), {}, done.get_continuation()});
    concore::spawn(concore::task{std::move(f3), {}, done.get_continuation()});

    done.wait();
    done.wait(); // should not wait on something else
    done.wait(); // should not wait on something else
    done.wait(); // should not wait on something else

    CHECK(work1_done.load());
    CHECK(work2_done.load());
    CHECK(work3_done.load());
}

TEST_CASE("finish_task using supplied executor", "[finish_task]") {

    std::atomic<bool> is_done{false};
    std::atomic<int> counter{0};
    auto fun = [&counter](task t) {
        counter++;
        t();
    };
    concore::delegating_executor e{std::move(fun)};
    concore::finish_task done_task([&] { is_done = true; }, e);

    CHECK(counter.load() == 0);
    CHECK(!is_done.load());
    done_task.get_continuation()({});
    CHECK(counter.load() == 1);
    CHECK(is_done.load());

    // Notifying for done does not have any effect on the task
    done_task.event().notify_done();
    CHECK(counter.load() == 1);
    CHECK(is_done.load());
}
