#include <catch2/catch.hpp>
#include <concore/detail/worker_tasks.hpp>
#include <concore/profiling.hpp>

#include "test_common/task_countdown.hpp"

#include <thread>
#include <chrono>
#include <array>

using namespace std::chrono_literals;

TEST_CASE("worker_tasks: pushing then popping behave like a stack", "[worker_tasks]") {
    constexpr int num_tasks = 100;

    // All the tasks would write some value here
    int out_location = -1;

    concore::detail::worker_tasks tasks;

    // Push a lot of tasks
    for (int i = 0; i < num_tasks; i++) {
        tasks.push([&out_location, i]() { out_location = i; });
    }

    // Pop them, and ensure they come up in reverse order
    concore::task extracted_task;
    for (int i = 0; i < num_tasks; i++) {
        REQUIRE(tasks.try_pop(extracted_task));

        // Running the task should write a number in out_location
        out_location = -1;
        extracted_task();
        REQUIRE(out_location == num_tasks - i - 1);
    }

    // Any more try_pop should fail
    REQUIRE_FALSE(tasks.try_pop(extracted_task));
}

TEST_CASE("worker_tasks: stealing gets far away tasks", "[worker_tasks]") {
    constexpr int num_tasks = 100;

    // All the tasks would write some value here
    int out_location = -1;

    concore::detail::worker_tasks tasks;

    // Push a lot of tasks
    for (int i = 0; i < num_tasks; i++) {
        tasks.push([&out_location, i]() { out_location = i; });
    }

    // Steal them, and ensure they come up in reverse order
    concore::task extracted_task;
    for (int i = 0; i < num_tasks; i++) {
        REQUIRE(tasks.try_steal(extracted_task));

        // Running the task should write a number in out_location
        out_location = -1;
        extracted_task();
        REQUIRE(out_location == i);
    }

    // Any more try_steal should fail
    REQUIRE_FALSE(tasks.try_steal(extracted_task));
}

TEST_CASE("worker_tasks: push,push,pop,steal cycles leave the stack empty", "[worker_tasks]") {
    constexpr int num_tasks = 100;

    // All the tasks would write some value here
    int out_location = -1;

    concore::detail::worker_tasks tasks;

    concore::task extracted_task;

    for (int i = 0; i < num_tasks; i++) {
        tasks.push([&out_location, i]() { out_location = i; });
        tasks.push([&out_location, i]() { out_location = 2 * i; });

        REQUIRE(tasks.try_pop(extracted_task));
        extracted_task();
        REQUIRE(out_location == 2 * i);
        REQUIRE(tasks.try_steal(extracted_task));
        extracted_task();
        REQUIRE(out_location == i);
    }

    // No tasks anymore
    REQUIRE_FALSE(tasks.try_pop(extracted_task));

    // One more time, but steal before pop
    for (int i = 0; i < num_tasks; i++) {
        tasks.push([&out_location, i]() { out_location = i; });
        tasks.push([&out_location, i]() { out_location = 2 * i; });

        REQUIRE(tasks.try_steal(extracted_task));
        extracted_task();
        REQUIRE(out_location == i);
        REQUIRE(tasks.try_pop(extracted_task));
        extracted_task();
        REQUIRE(out_location == 2 * i);
    }

    // No tasks anymore
    REQUIRE_FALSE(tasks.try_pop(extracted_task));
}

TEST_CASE("worker_tasks: can steal in parallel with push", "[worker_tasks]") {
    constexpr int num_tasks = 1'000;

    std::array<bool, num_tasks> res{};
    res.fill(false);

    task_countdown barrier{2};

    concore::detail::worker_tasks tasks;
    std::atomic<int> num_active_tasks{0};

    std::thread producer = std::thread([&]() {
        barrier.task_finished();
        barrier.wait_for_all();

        for (int i = 0; i < num_tasks; i++) {
            tasks.push([&res, i]() { res[i] = true; });

            // Don't let us accumulate too many tasks, so that the performance of the thief doesn't
            // decrease quadratically.
            num_active_tasks++;
            concore::spin_backoff spinner;
            while (num_active_tasks.load() > 10)
                spinner.pause();
        }
    });

    std::thread thief = std::thread([&]() {
        barrier.task_finished();
        barrier.wait_for_all();

        concore::task extracted_task;
        for (int i = 0; i < num_tasks; i++) {
            concore::spin_backoff spinner;
            while (true) {
                if (tasks.try_steal(extracted_task)) {
                    extracted_task();
                    num_active_tasks--;
                    break;
                }
                spinner.pause();
            }
        }
    });

    // Wait for both threads to finish
    producer.join();
    thief.join();

    // Check that all the tasks have been executed
    for (int i = 0; i < num_tasks; i++)
        REQUIRE(res[i]);
}

TEST_CASE("worker_tasks: can steal in parallel with push/pop", "[worker_tasks]") {
    constexpr int num_tasks = 2'000;

    std::array<bool, num_tasks> res{};
    res.fill(false);

    task_countdown barrier{2};

    concore::detail::worker_tasks tasks;
    std::atomic<int> num_extracted_tasks{0};

    std::thread producer = std::thread([&]() {
        barrier.task_finished();
        barrier.wait_for_all();

        concore::task extracted_task;
        for (int i = 0; i < num_tasks; i++) {
            tasks.push([&res, i]() { res[i] = true; });

            // Every two pushes, do a pop
            if ((i % 2) == 1) {
                if (tasks.try_pop(extracted_task)) {
                    extracted_task();
                    num_extracted_tasks++;
                }
            }

            // Don't let us accumulate too many tasks, so that the performance of the thief doesn't
            // decrease quadratically.
            concore::spin_backoff spinner;
            while (i / 2 - num_extracted_tasks.load() > 10)
                spinner.pause();
        }
    });

    std::thread thief = std::thread([&]() {
        barrier.task_finished();
        barrier.wait_for_all();

        concore::task extracted_task;
        concore::spin_backoff spinner;
        while (num_extracted_tasks.load() < num_tasks) {
            if (tasks.try_steal(extracted_task)) {
                extracted_task();
                num_extracted_tasks++;

                spinner = concore::spin_backoff();
            } else
                spinner.pause();
        }
    });

    // Wait for both threads to finish
    producer.join();
    thief.join();

    // Check that all the tasks have been executed
    for (int i = 0; i < num_tasks; i++)
        REQUIRE(res[i]);
}
