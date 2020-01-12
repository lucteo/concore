#include <catch2/catch.hpp>
#include <concore/data/concurrent_queue.hpp>
#include <concore/profiling.hpp>

#include "test_common/task_countdown.hpp"

#include <thread>
#include <chrono>

using namespace std::chrono_literals;

using concore::queue_type;

template <concore::queue_type conc_type>
void test_pushes_then_pops() {
    CONCORE_PROFILING_FUNCTION();

    constexpr int num_elements = 1000;

    concore::concurrent_queue<int, conc_type> queue;

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
    SECTION("single_prod_single_cons") {
        test_pushes_then_pops<queue_type::single_prod_single_cons>();
    }
    SECTION("single_prod_multi_cons") {
        test_pushes_then_pops<queue_type::single_prod_multi_cons>();
    }
    SECTION("multi_prod_single_cons") {
        test_pushes_then_pops<queue_type::multi_prod_single_cons>();
    }
    SECTION("multi_prod_multi_cons") { test_pushes_then_pops<queue_type::multi_prod_multi_cons>(); }
}

template <concore::queue_type conc_type>
void test_one_pusher_one_popper() {
    CONCORE_PROFILING_FUNCTION();

    constexpr int num_elements = 10'000;

    concore::concurrent_queue<int, conc_type> queue;

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
    SECTION("single_prod_single_cons") {
        test_one_pusher_one_popper<queue_type::single_prod_single_cons>();
    }
    SECTION("single_prod_multi_cons") {
        test_one_pusher_one_popper<queue_type::single_prod_multi_cons>();
    }
    SECTION("multi_prod_single_cons") {
        test_one_pusher_one_popper<queue_type::multi_prod_single_cons>();
    }
    SECTION("multi_prod_multi_cons") {
        test_one_pusher_one_popper<queue_type::multi_prod_multi_cons>();
    }
}

template <concore::queue_type conc_type>
void test_multi_threads() {
    CONCORE_PROFILING_FUNCTION();

    concore::concurrent_queue<int, conc_type> queue;

    const int num_pushing_threads = conc_type == queue_type::single_prod_single_cons ||
                                                    conc_type == queue_type::single_prod_multi_cons
                                            ? 1
                                            : 3;
    const int num_popping_threads = conc_type == queue_type::single_prod_single_cons ||
                                                    conc_type == queue_type::multi_prod_single_cons
                                            ? 1
                                            : 3;
    const int num_threads = num_pushing_threads + num_popping_threads;

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
    int idx = 0;
    for (int i = 0; i < num_pushing_threads; i++)
        threads[idx++] = std::thread{work_fun, true};
    for (int i = 0; i < num_popping_threads; i++)
        threads[idx++] = std::thread{work_fun, false};

    // Wait for all the threads to finish
    for (int i = 0; i < num_threads; i++)
        threads[i].join();
    REQUIRE(true);
}

TEST_CASE("concurrent_queue: multiple threads pushing and popping", "[concurrent_queue]") {
    SECTION("single_prod_single_cons") {
        test_multi_threads<queue_type::single_prod_single_cons>();
    }
    SECTION("single_prod_multi_cons") { test_multi_threads<queue_type::single_prod_multi_cons>(); }
    SECTION("multi_prod_single_cons") { test_multi_threads<queue_type::multi_prod_single_cons>(); }
    SECTION("multi_prod_multi_cons") { test_multi_threads<queue_type::multi_prod_multi_cons>(); }
}

std::atomic<int> g_num_ctors_{0};
std::atomic<int> g_num_copy_ctors_{0};
std::atomic<int> g_num_move_ctors_{0};
std::atomic<int> g_num_dtors_{0};
std::atomic<int> g_num_copy_oper_{0};
std::atomic<int> g_num_move_oper_{0};

void reset_counters() {
    g_num_ctors_ = 0;
    g_num_copy_ctors_ = 0;
    g_num_move_ctors_ = 0;
    g_num_dtors_ = 0;
    g_num_copy_oper_ = 0;
    g_num_move_oper_ = 0;
}

struct my_obj {
    my_obj() { g_num_ctors_++; }
    my_obj(const my_obj&) { g_num_copy_ctors_++; }
    my_obj(my_obj&&) { g_num_move_ctors_++; }
    ~my_obj() { g_num_dtors_++; }
    void operator=(const my_obj&) { g_num_copy_oper_++; }
    void operator=(my_obj&&) { g_num_move_oper_++; }
};

template <concore::queue_type conc_type>
void test_ctor_dtor() {
    constexpr int num_elements = 100;

    reset_counters();
    concore::concurrent_queue<my_obj> queue;

    // Push some elements in the queue
    for (int i = 0; i < num_elements; i++) {
        queue.push(my_obj());
    }
    REQUIRE(g_num_ctors_.load() == num_elements);
    REQUIRE(g_num_copy_ctors_.load() == 0);
    REQUIRE(g_num_move_ctors_.load() == num_elements);
    REQUIRE(g_num_dtors_.load() == num_elements);
    REQUIRE(g_num_copy_oper_.load() == 0);
    REQUIRE(g_num_move_oper_.load() == 0);

    // Pop now all the elements
    my_obj obj;
    for (int i = 0; i < num_elements; i++) {
        bool res = queue.try_pop(obj);
    }
    REQUIRE(g_num_ctors_.load() == num_elements + 1);
    REQUIRE(g_num_copy_ctors_.load() == 0);
    REQUIRE(g_num_move_ctors_.load() == num_elements);
    REQUIRE(g_num_dtors_.load() == 2 * num_elements);
    REQUIRE(g_num_copy_oper_.load() == 0);
    REQUIRE(g_num_move_oper_.load() == num_elements);
}

TEST_CASE("concurrent_queue: ctors and dtors", "[concurrent_queue]") {
    SECTION("single_prod_single_cons") { test_ctor_dtor<queue_type::single_prod_single_cons>(); }
    SECTION("single_prod_multi_cons") { test_ctor_dtor<queue_type::single_prod_multi_cons>(); }
    SECTION("multi_prod_single_cons") { test_ctor_dtor<queue_type::multi_prod_single_cons>(); }
    SECTION("multi_prod_multi_cons") { test_ctor_dtor<queue_type::multi_prod_multi_cons>(); }
}