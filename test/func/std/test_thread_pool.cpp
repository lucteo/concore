#include <catch2/catch.hpp>
#include <concore/std/thread_pool.hpp>
#include "test_common/task_utils.hpp"

#include <array>
#include <atomic>
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
        "[execution]") {
    static_thread_pool my_pool{4};
    REQUIRE_FALSE(my_pool.executor().running_in_this_thread());
}
TEST_CASE("static_thread_pool executor's running_in_this_thread returns true if called from inside "
          "a running task",
        "[execution]") {
    static_thread_pool my_pool{4};
    auto ex = my_pool.executor();
    ex.execute([&] { REQUIRE(ex.running_in_this_thread()); });
    // wait for the task to be executed
    auto grp = detail::get_associated_group(my_pool);
    REQUIRE(bounded_wait(grp));
}

int get_max_concurrency(static_thread_pool& pool, int num_tasks) {
    std::atomic<int> num_parallel{0};
    std::vector<int> results;
    results.resize(num_tasks, 0);

    std::atomic<bool> can_continue{false};

    // Start all the tasks in parallel; but keep them blocked
    auto ex = pool.executor();
    for (int i = 0; i < num_tasks; i++) {
        ex.execute([i, &results, &can_continue, &num_parallel] {
            // Increase the parallelism and note the current parallelism level
            results[i] = ++num_parallel;
            // Wait until all the tasks are started
            while (!can_continue.load())
                std::this_thread::sleep_for(100us);
            // Decrease parallelism and exit
            num_parallel--;
        });
    }
    std::this_thread::sleep_for(1ms);

    // Unblock all the tasks
    can_continue = true;

    // wait for the tasks to be executed
    auto grp = detail::get_associated_group(pool);
    REQUIRE(bounded_wait(grp));

    // Check that the maximum parallelism is the one we specified
    int max_concurrency = 0;
    for (auto res : results) {
        if (max_concurrency < res)
            max_concurrency = res;
    }
    return max_concurrency;
}

TEST_CASE("static_thread_pool cannot execute more than maximum concurrency tasks in parallel",
        "[execution]") {
    constexpr int num_threads = 10;
    static_thread_pool my_pool{num_threads};
    int conc = get_max_concurrency(my_pool, 2 * num_threads);
    REQUIRE(conc <= num_threads);
}

TEST_CASE("static_thread_pool::attach will make the calling thread join the pool", "[execution]") {
    std::thread* new_thread{nullptr};
    {
        static_thread_pool my_pool{1};

        // Create a new thread that joins the pool
        new_thread = new std::thread([&] { my_pool.attach(); });
        std::this_thread::sleep_for(1ms);
        auto tid = new_thread->get_id();

        // Add a lot of tasks to the pool, until we have one executed by our thread
        std::atomic<bool> found{false};
        auto ex = my_pool.executor();
        for (int i = 0; i < 100; i++) {
            ex.execute([&] {
                if (!found) {
                    if (std::this_thread::get_id() == tid) {
                        found = true;
                        REQUIRE(ex.running_in_this_thread());
                    } else {
                        // Give the extra worker thread a chance to run
                        std::this_thread::sleep_for(1ms);
                    }
                }
            });
        }
        // wait for the tasks to be executed
        auto grp = detail::get_associated_group(my_pool);
        REQUIRE(bounded_wait(grp));
        // We should have at least one task running in our new thread
        REQUIRE(found.load() == true);
    }
    // After the thread is released from the pool, join and delete the object
    new_thread->join();
    delete new_thread;
}
TEST_CASE("static_thread_pool::attach will increase the number of threads in the pool",
        "[execution]") {
    constexpr int num_pool_threads = 2;
    constexpr int num_extra_threads = 4;
    std::vector<std::thread> extra_threads{};
    extra_threads.reserve(num_extra_threads);
    {
        // Create the thread pool
        static_thread_pool my_pool{num_pool_threads};

        // Add some extra threads to the pool
        for (int i = 0; i < num_extra_threads; i++)
            extra_threads.emplace_back(std::thread([&] { my_pool.attach(); }));

        // Check the max concurrency of the pool
        int conc = get_max_concurrency(my_pool, 100);
        REQUIRE(conc <= num_pool_threads + num_extra_threads);
        REQUIRE(conc > num_pool_threads);
    }
    // Join the threads
    for (auto& t : extra_threads)
        t.join();
}
TEST_CASE("static_thread_pool attached thread will continue after the thread pool is stopped",
        "[execution]") {
    std::thread extra_thread{};
    std::atomic<bool> after_attach{false};
    {
        static_thread_pool my_pool{1};

        // Add thread too the pool
        extra_thread = std::thread([&] {
            my_pool.attach();
            after_attach = true;
        });

        // Add a bunch of tasks to the pool
        constexpr int num_tasks = 100;
        std::atomic<int> num_executed{0};
        auto ex = my_pool.executor();
        ex.bulk_execute([&](int) { num_executed++; }, num_tasks);

        // Wait for all the tasks to be executed
        auto grp = detail::get_associated_group(my_pool);
        REQUIRE(bounded_wait(grp));
        REQUIRE(num_executed.load() == num_tasks);

        // At this point, the extra thread is still inside the call to 'attach()'
        REQUIRE_FALSE(after_attach.load());
    }

    // Wait until we exit the attach() function and we set the atomic
    for (int i = 0; i < 100; i++) {
        if (!after_attach.load())
            std::this_thread::sleep_for(1ms);
    }
    REQUIRE(after_attach.load());

    // Join thee thread
    extra_thread.join();
}
TEST_CASE("static_thread_pool does not take new tasks after stop()", "[execution]") {}
TEST_CASE("static_thread_pool does not take new tasks after wait()", "[execution]") {}
TEST_CASE("static_thread_pool waits for all the tasks to be done when calling wait()",
        "[execution]") {}
