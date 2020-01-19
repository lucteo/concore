#include <catch2/catch.hpp>
#include <concore/low_level/concurrent_dequeue.hpp>
#include <concore/profiling.hpp>

#include "test_common/task_countdown.hpp"

#include <thread>
#include <chrono>

using namespace std::chrono_literals;

void test_pushes_then_pops(int fast_size, int num_elements, bool push_front, bool pop_front) {
    CONCORE_PROFILING_FUNCTION();

    concore::concurrent_dequeue<int> queue{static_cast<size_t>(fast_size)};

    // Push the elements to the queue
    for (int i = 0; i < num_elements; i++) {
        if (push_front)
            queue.push_front(int(i));
        else
            queue.push_back(int(i));
    }

    // Now, we can extract all these elements, in the right order
    for (int i = 0; i < num_elements; i++) {
        int value;
        if (pop_front)
            REQUIRE(queue.try_pop_front(value));
        else
            REQUIRE(queue.try_pop_back(value));

        bool same_order = push_front != pop_front;
        bool only_fast = num_elements < fast_size;
        if (same_order)
            REQUIRE(value == i);
        else if (only_fast)
            REQUIRE(value == num_elements - i - 1);
        else {
            REQUIRE(value >= 0);
            REQUIRE(value < num_elements);
        }
    }

    // Trying to pop from either side now, should return false
    int value;
    REQUIRE_FALSE(queue.try_pop_front(value));
    REQUIRE_FALSE(queue.try_pop_back(value));
}

TEST_CASE("concurrent_dequeue: pushing then popping yields the same elements; different ends, "
          "different orders",
        "[concurrent_dequeue]") {
    CONCORE_PROFILING_FUNCTION();
    SECTION("push_back, pop_front") {
        test_pushes_then_pops(100, 20, false, true);
        test_pushes_then_pops(20, 100, false, true);
    }
    SECTION("push_back, pop_back") {
        test_pushes_then_pops(100, 20, false, false);
        test_pushes_then_pops(20, 100, false, false);
    }
    SECTION("push_front, pop_front") {
        test_pushes_then_pops(100, 20, true, true);
        test_pushes_then_pops(20, 100, true, true);
    }
    SECTION("push_front, pop_back") {
        test_pushes_then_pops(100, 20, true, false);
        test_pushes_then_pops(20, 100, true, false);
    }
}

void test_one_pusher_one_popper(int fast_size, int num_elements, bool push_front, bool pop_front) {
    CONCORE_PROFILING_FUNCTION();

    concore::concurrent_dequeue<int> queue{static_cast<size_t>(fast_size)};

    // used for a synchronized start
    task_countdown barrier{2};

    auto pusher_work = [=, &queue, &barrier]() {
        barrier.task_finished();
        barrier.wait_for_all();

        // Push the elements to the queue
        for (int i = 0; i < num_elements; i++) {
            if (push_front)
                queue.push_front(int(i));
            else
                queue.push_back(int(i));
        }
    };

    auto popper_work = [=, &queue, &barrier]() {
        barrier.task_finished();
        barrier.wait_for_all();

        // Continuously extract elements; do a busy-loop
        int pops_remaining = num_elements;
        while (pops_remaining > 0) {
            int value;
            bool popped = pop_front ? queue.try_pop_front(value) : queue.try_pop_back(value);

            if (popped) {
                // Ensure that the value is within the range
                REQUIRE(value >= 0);
                REQUIRE(value < num_elements);
                pops_remaining--;
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

TEST_CASE("concurrent_dequeue: one thread pushes, one thread pops", "[concurrent_dequeue]") {
    CONCORE_PROFILING_FUNCTION();
    SECTION("push_back, pop_front") { test_one_pusher_one_popper(1024, 10'000, false, true); }
    SECTION("push_back, pop_back") { test_one_pusher_one_popper(1024, 10'000, false, false); }
    SECTION("push_front, pop_front") { test_one_pusher_one_popper(1024, 10'000, true, true); }
    SECTION("push_front, pop_back") { test_one_pusher_one_popper(1024, 10'000, true, false); }
}

TEST_CASE("concurrent_dequeue: multiple threads pushing and popping continuously",
        "[concurrent_dequeue]") {
    CONCORE_PROFILING_FUNCTION();

    concore::concurrent_dequeue<int> queue{static_cast<size_t>(20)};

    constexpr int num_threads_per_type = 3;
    constexpr int num_threads = num_threads_per_type * 4;

    // used for a synchronized start
    task_countdown barrier{num_threads};

    auto start = std::chrono::high_resolution_clock::now();
    auto end = start + 100ms;

    auto work_fun = [=, &queue, &barrier](int idx, bool pusher, bool front) {
        barrier.task_finished();
        barrier.wait_for_all();

        int i = 0;
        int value;
        while (std::chrono::high_resolution_clock::now() < end) {
            if (pusher && front)
                queue.push_front(i++);
            else if (pusher && !front)
                queue.push_back(i++);
            else if (!pusher && front)
                queue.try_pop_front(value);
            else if (!pusher && !front)
                queue.try_pop_back(value);
        }
    };

    // Start all the threads
    std::vector<std::thread> threads{num_threads};
    for (int i = 0; i < num_threads_per_type; i++) {
        threads[i * 4 + 0] = std::thread{work_fun, i * 4 + 0, true, false};
        threads[i * 4 + 1] = std::thread{work_fun, i * 4 + 1, true, true};
        threads[i * 4 + 2] = std::thread{work_fun, i * 4 + 2, false, true};
        threads[i * 4 + 3] = std::thread{work_fun, i * 4 + 3, false, false};
    }

    // Wait for all the threads to finish
    for (int i = 0; i < num_threads; i++)
        threads[i].join();
    REQUIRE(true);
}
