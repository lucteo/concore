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
        int value = 0;
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
    int value{0};
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
            int value{0};
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

    concore::concurrent_dequeue<int> queue{static_cast<size_t>(1000)};

    constexpr int num_push_back_threads = 3;
    constexpr int num_push_front_threads = 3;
    constexpr int num_pop_back_threads = 3;
    constexpr int num_pop_front_threads = 3;
    constexpr int num_threads = num_push_back_threads + num_push_front_threads +
                                num_pop_back_threads + num_pop_front_threads;

    // used for a synchronized start
    task_countdown barrier{num_threads};

    auto start = std::chrono::high_resolution_clock::now();
    auto end = start + 100ms;

    std::atomic<int> num_pushes{0};
    std::atomic<int> num_pops{0};

    auto work_fun = [=, &queue, &barrier, &num_pushes, &num_pops](
                            int idx, bool pusher, bool front) {
        barrier.task_finished();
        barrier.wait_for_all();

        int i = 0;
        int value{0};
        while (std::chrono::high_resolution_clock::now() < end) {
            if (pusher) {
                if (front)
                    queue.push_front(i++);
                else
                    queue.push_back(i++);
                num_pushes++;
            } else {
                bool pop_succeeded = false;
                if (front)
                    pop_succeeded = queue.try_pop_front(value);
                else
                    pop_succeeded = queue.try_pop_back(value);
                if (pop_succeeded)
                    num_pops++;
            }
        }
    };

    // Start all the threads
    std::vector<std::thread> threads{num_threads};
    int idx = 0;
    for (int i = 0; i < num_push_back_threads; i++)
        threads[idx++] = std::thread{work_fun, idx, true, false};
    for (int i = 0; i < num_push_front_threads; i++)
        threads[idx++] = std::thread{work_fun, idx, true, true};
    for (int i = 0; i < num_push_back_threads; i++)
        threads[idx++] = std::thread{work_fun, idx, false, false};
    for (int i = 0; i < num_push_back_threads; i++)
        threads[idx++] = std::thread{work_fun, idx, false, true};

    // Wait for all the threads to finish
    for (int i = 0; i < num_threads; i++)
        threads[i].join();

    // After all the threads are done, we might get some additional items in the queue; pop them
    int value{0};
    while (queue.try_pop_front(value))
        num_pops++;

    // We should get the same number of pushes and of pops
    int pushes = num_pushes.load();
    int pops = num_pops.load();
    CHECK(pushes == pops);
}
