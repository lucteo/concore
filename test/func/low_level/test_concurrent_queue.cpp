#include <catch2/catch.hpp>
#include <concore/low_level/concurrent_queue.hpp>
#include <concore/profiling.hpp>

#include "test_common/task_countdown.hpp"

#include <thread>
#include <chrono>

using namespace std::chrono_literals;

void test_pushes_then_pops(int num_elements) {
    CONCORE_PROFILING_FUNCTION();

    concore::concurrent_queue<int> queue;

    // Push the elements to the queue
    for (int i = 0; i < num_elements; i++) {
        queue.push(int(i));
    }

    // Now, we can extract all these elements, in the right order
    for (int i = 0; i < num_elements; i++) {
        int value;
        REQUIRE(queue.try_pop(value));
        REQUIRE(value == i);
    }

    // Another pop should not return false
    int value;
    REQUIRE_FALSE(queue.try_pop(value));
}

TEST_CASE("concurrent_queue: pushing then popping yields the same elements", "[concurrent_queue]") {
    CONCORE_PROFILING_FUNCTION();
    test_pushes_then_pops(20);
    test_pushes_then_pops(100);
}

void test_one_pusher_one_popper(int num_elements) {
    CONCORE_PROFILING_FUNCTION();

    concore::concurrent_queue<int> queue;

    // used for a synchronized start
    task_countdown barrier{2};

    auto pusher_work = [=, &queue, &barrier]() {
        barrier.task_finished();
        barrier.wait_for_all();

        // Push the elements to the queue
        for (int i = 0; i < num_elements; i++)
            queue.push(int(i));
    };

    auto popper_work = [=, &queue, &barrier]() {
        barrier.task_finished();
        barrier.wait_for_all();

        // Continuously extract elements; do a busy-loop
        int idx = 0;
        while (idx < num_elements) {
            int value;
            if (queue.try_pop(value)) {
                // Ensure that the value is as expected
                REQUIRE(value == idx++);
            }
        }
    };

    // Start the two threads
    std::thread pusher{pusher_work};
    std::thread popper{popper_work};

    // Wait for the threads to finish
    pusher.join();
    popper.join();
}

TEST_CASE("concurrent_queue: one thread pushes, one thread pops", "[concurrent_queue]") {
    CONCORE_PROFILING_FUNCTION();
    test_one_pusher_one_popper(10'000);
}

TEST_CASE("concurrent_queue: multiple threads pushing, one popping", "[concurrent_queue]") {
    CONCORE_PROFILING_FUNCTION();

    concore::concurrent_queue<int> queue;

    constexpr int num_pushing_threads = 3;
    constexpr int num_threads = 1 + num_pushing_threads;

    // used for a synchronized start
    task_countdown barrier{num_threads};

    auto start = std::chrono::high_resolution_clock::now();
    auto end = start + 100ms;

    auto work_fun = [=, &queue, &barrier](bool pusher) {
        barrier.task_finished();
        barrier.wait_for_all();

        int i = 0;
        int value;
        while (std::chrono::high_resolution_clock::now() < end) {
            if (pusher)
                queue.push(i++);
            else
                queue.try_pop(value);
        }
    };

    // Start all the threads
    std::vector<std::thread> threads{num_threads};
    threads[0] = std::thread{work_fun, false};
    for (int i = 1; i <= num_pushing_threads; i++)
        threads[i] = std::thread{work_fun, true};

    // Wait for all the threads to finish
    for (int i = 0; i < num_threads; i++)
        threads[i].join();
    REQUIRE(true);
}
