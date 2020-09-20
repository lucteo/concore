#include <catch2/catch.hpp>
#include <concore/serializer.hpp>
#include <concore/n_serializer.hpp>
#include <concore/rw_serializer.hpp>
#include <concore/global_executor.hpp>
#include <concore/spawn.hpp>

#include "test_common/task_countdown.hpp"
#include "test_common/task_utils.hpp"

#include <stdexcept>
#include <cstdlib>
#include <ctime>
#include <array>

namespace {

//! Check if the given executor can execute tasks.
void check_execute_tasks(concore::executor_t e) {
    int num_tasks = 10;
    std::atomic<int> counter{0};
    REQUIRE(enqueue_and_wait(
            e, [&]() { counter++; }, num_tasks));
    REQUIRE(counter == num_tasks);
}

//! Executor that counts how many tasks it has executed.
struct counting_executor {
    std::shared_ptr<std::atomic<int>> count_{std::make_shared<std::atomic<int>>(0)};
    void operator()(concore::task t) {
        (*count_)++;
        concore::global_executor(std::move(t));
    }
};

using ex_fun_t = std::function<void(std::exception_ptr)>;

//! Check that the given executor can execute tasks in the presence of exceptions.
//! `creat` is a function that takes an `ex_fun_t` and returns an executor.
template <typename Creator>
void check_execute_with_exceptions(Creator creat) {
    constexpr int num_tasks = 10;
    task_countdown tc{num_tasks};

    std::atomic<int> num_exceptions{0};
    auto except_fun = [&](std::exception_ptr) {
        num_exceptions++;
        tc.task_finished();
    };
    auto grp = concore::task_group::create();
    grp.set_exception_handler(except_fun);

    auto executor = creat();

    // Create the tasks, and add them to the executor
    for (int i = 0; i < num_tasks; i++) {
        concore::task t{[]() { throw std::logic_error("something went wrong"); }, grp};
        executor(std::move(t));
    }

    // Wait for all the tasks to complete
    REQUIRE(tc.wait_for_all());

    // Ensure that the exception function was called
    REQUIRE(num_exceptions == num_tasks);
}

struct throwing_executor {
    void operator()(concore::task&& t) {
        concore::spawn(std::move(t));
        throw std::logic_error("err");
    }
};

//! Checks that executing tasks on the given executor, will have the desired level of parallelism:
//!     - always <= max_par
//!     - at least once >= min-par
void check_parallelism(concore::executor_t e, int max_par, int min_par = 1) {
    constexpr int num_tasks = 10;
    task_countdown tc{num_tasks};

    std::array<int, num_tasks> results{};
    std::atomic<int> end_idx{0};
    std::atomic<int> cur_parallelism{0};

    // Create the tasks, and add them to the executor
    for (int i = 0; i < num_tasks; i++)
        e([&]() {
            cur_parallelism++;
            std::this_thread::sleep_for(1ms);
            results[end_idx++] = cur_parallelism.load();
            std::this_thread::sleep_for(1ms);
            cur_parallelism--;
            tc.task_finished();
        });

    // Wait for all the tasks to complete
    REQUIRE(tc.wait_for_all());
    // No parallelism at the end
    REQUIRE(cur_parallelism.load() == 0);

    // Check the max parallelism
    for (int i = 0; i < num_tasks; i++)
        REQUIRE(results[i] <= max_par);

    // Check min parallelism
    int max_par_val = 0;
    for (int i = 0; i < num_tasks; i++) {
        if (results[i] > max_par_val)
            max_par_val = results[i];
    }
    REQUIRE(max_par_val >= min_par);
}

//! Check that the given executor can execute tasks in the order in which they are enqueued
//! (one at a time)
void check_in_order_execution(concore::executor_t e) {
    constexpr int num_tasks = 10;
    task_countdown tc{num_tasks};

    std::array<int, num_tasks> results{};
    std::atomic<int> end_idx{0};

    // Create the tasks, and add them to the executor
    for (int i = 0; i < num_tasks; i++)
        e([&, i]() {
            results[end_idx++] = i;
            tc.task_finished();
        });

    // Wait for all the tasks to complete
    REQUIRE(tc.wait_for_all());

    // Ensure the tasks are executed in order
    for (int i = 0; i < num_tasks; i++)
        REQUIRE(results[i] == i);
}

} // namespace

TEST_CASE("serializers are executors", "[ser]") {
    auto ge = concore::global_executor;
    auto task = []() {};

    SECTION("serializer is copyable") {
        auto e1 = concore::serializer();
        auto e2 = concore::serializer(ge);
        auto e3 = concore::serializer(e1);
        e2 = e1;
    }
    SECTION("serializer has execution syntax") {
        auto e = concore::serializer(ge);
        e(task);
    }

    SECTION("n_serializer is copyable") {
        auto e1 = concore::n_serializer(4);
        auto e2 = concore::n_serializer(4, ge);
        // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
        auto e3 = concore::n_serializer(e1);
        e2 = e1;
    }
    SECTION("n_serializer has execution syntax") {
        auto e = concore::n_serializer(4);
        e(task);
    }

    SECTION("rw_serializer.reader is copyable") {
        auto e1 = concore::rw_serializer().reader();
        auto e2 = concore::rw_serializer().reader();
        // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
        concore::rw_serializer::reader_type e3(e1);
        e2 = e1;
    }
    SECTION("rw_serializer.reader has execution syntax") {
        auto e = concore::rw_serializer().reader();
        e(task);
    }

    SECTION("rw_serializer.writer is copyable") {
        auto e1 = concore::rw_serializer().writer();
        auto e2 = concore::rw_serializer().writer();
        // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
        concore::rw_serializer::writer_type e3(e1);
        e2 = e1;
    }
    SECTION("rw_serializer.writer has execution syntax") {
        auto e = concore::rw_serializer().writer();
        e(task);
    }
}

TEST_CASE("Tasks added to serializers are executed", "[ser]") {
    SECTION("serializer executes tasks") { check_execute_tasks(concore::serializer()); }
    SECTION("n_serializer executes tasks") { check_execute_tasks(concore::n_serializer(4)); }
    SECTION("rw_serializer.reader executes tasks") {
        check_execute_tasks(concore::rw_serializer().reader());
    }
    SECTION("rw_serializer.writer executes tasks") {
        check_execute_tasks(concore::rw_serializer().writer());
    }
}

TEST_CASE("Serializers use the given executors", "[ser]") {
    auto f = []() { std::this_thread::sleep_for(1ms); };
    SECTION("serializer and executors") {
        counting_executor cnt_exe1, cnt_exe2;
        auto e = concore::serializer(cnt_exe1, cnt_exe2);
        REQUIRE(enqueue_and_wait(e, f));
        REQUIRE(cnt_exe1.count_->load() == 1);
        REQUIRE(cnt_exe2.count_->load() == 9);
    }
    SECTION("n_serializer and executors") {
        counting_executor cnt_exe1, cnt_exe2;
        auto e = concore::n_serializer(3, cnt_exe1, cnt_exe2);
        REQUIRE(enqueue_and_wait(e, f));
        REQUIRE(cnt_exe1.count_->load() == 3);
        REQUIRE(cnt_exe2.count_->load() == 7);
    }
    SECTION("rw_serializer.reader and executors") {
        counting_executor cnt_exe1, cnt_exe2;
        concore::rw_serializer rw_ser(cnt_exe1, cnt_exe2);
        auto e = rw_ser.reader();
        REQUIRE(enqueue_and_wait(e, f));
        REQUIRE(cnt_exe1.count_->load() == 10);
        REQUIRE(cnt_exe2.count_->load() == 0);
    }
    SECTION("rw_serializer.writer and executors") {
        counting_executor cnt_exe1, cnt_exe2;
        concore::rw_serializer rw_ser(cnt_exe1, cnt_exe2);
        auto e = rw_ser.writer();
        REQUIRE(enqueue_and_wait(e, f));
        REQUIRE(cnt_exe1.count_->load() == 1);
        REQUIRE(cnt_exe2.count_->load() == 9);
    }
}

TEST_CASE("Serializers can execute tasks with exceptions", "[ser]") {
    SECTION("serializer executes tasks with exceptions") {
        auto creat = []() -> auto { return concore::serializer(concore::global_executor); };
        check_execute_with_exceptions(creat);
    }
    SECTION("n_serializer executes tasks with exceptions") {
        auto creat = []() -> auto { return concore::n_serializer(4, concore::global_executor); };
        check_execute_with_exceptions(creat);
    }
    SECTION("rw_serializer.reader executes tasks with exceptions") {
        auto creat = []() -> auto {
            return concore::rw_serializer(concore::global_executor).reader();
        };
        check_execute_with_exceptions(creat);
    }
    SECTION("rw_serializer.writer executes tasks with exceptions") {
        auto creat = []() -> auto {
            return concore::rw_serializer(concore::global_executor).writer();
        };
        check_execute_with_exceptions(creat);
    }
}

TEST_CASE("Serializers work if the executor throws exceptions", "[ser]") {
    SECTION("serializer works if the executor throws exceptions") {
        auto ser = concore::serializer(throwing_executor());
        std::atomic<int> num_ex = 0;
        ser.set_exception_handler([&](std::exception_ptr) { num_ex++; });
        check_execute_tasks(ser);
        REQUIRE(num_ex.load() == 10);
    }
    SECTION("n_serializer works if the executor throws exceptions") {
        auto ser = concore::n_serializer(4, throwing_executor());
        std::atomic<int> num_ex = 0;
        ser.set_exception_handler([&](std::exception_ptr) { num_ex++; });
        check_execute_tasks(ser);
        REQUIRE(num_ex.load() == 10);
    }
    SECTION("rw_serializer.reader works if the executor throws exceptions") {
        auto ser = concore::rw_serializer(throwing_executor());
        std::atomic<int> num_ex = 0;
        ser.set_exception_handler([&](std::exception_ptr) { num_ex++; });
        check_execute_tasks(ser.reader());
        REQUIRE(num_ex.load() == 10);
    }
    SECTION("rw_serializer.writer works if the executor throws exceptions") {
        auto ser = concore::rw_serializer(throwing_executor());
        std::atomic<int> num_ex = 0;
        ser.set_exception_handler([&](std::exception_ptr) { num_ex++; });
        check_execute_tasks(ser.writer());
        REQUIRE(num_ex.load() == 10);
    }
}

TEST_CASE("Serializers obey maximum allowed parallelism", "[ser]") {
    SECTION("1 task at a time for a serializer") { check_parallelism(concore::serializer(), 1); }
    SECTION("N task at a time for an n_serializer") {
        check_parallelism(concore::n_serializer(2), 2);
        check_parallelism(concore::n_serializer(4), 4);
    }
    SECTION("1 task at a time for a rw_serializer.writer") {
        check_parallelism(concore::rw_serializer().writer(), 1);
    }
    SECTION("1 task at a time for a rw_serializer.writer") {
        check_parallelism(concore::rw_serializer().writer(), 1);
    }
}

TEST_CASE("serializer executes tasks in order", "[ser]") {
    check_in_order_execution(concore::serializer(concore::global_executor));
}

TEST_CASE("n_serializer with N=1 behaves like a serializer", "[ser]") {
    check_in_order_execution(concore::n_serializer(1, concore::global_executor));
}

TEST_CASE("rw_serializer.writer behaves like a serializer", "[ser]") {
    check_in_order_execution(concore::rw_serializer(concore::global_executor).writer());
}

TEST_CASE("rw_serializer.reader has parallelism", "[ser]") {
    // This only works if we have multiple cores; use 4 cores to increase the chance of executing in
    // parallel
    if (std::thread::hardware_concurrency() < 4)
        return;

    check_parallelism(concore::rw_serializer().reader(), 10000, 2);
}

// Generate 1 WRITE and 9 READs; the WRITE will have a random order
// All READs issued before the WRITE will be executed before the WRITE
// All READs issued after the WRITE will be executed after the WRITE
TEST_CASE("rw_serializer will execute WRITEs as soon as possible", "[ser]") {
    auto rws = concore::rw_serializer(concore::global_executor);

    constexpr int num_tasks = 10;
    task_countdown tc{num_tasks};

    std::srand(std::time(0));
    int write_pos = std::rand() % num_tasks;

    std::array<int, num_tasks> results{};
    std::atomic<int> end_idx{0};

    // Create the tasks, and add them to the right executor
    for (int i = 0; i < num_tasks; i++) {
        auto e = i == write_pos ? concore::executor_t(rws.writer())
                                : concore::executor_t(rws.reader());
        e([&, i]() {
            results[end_idx++] = i;
            // Randomly wait a bit of time
            int rnd = 1 + std::rand() % 6; // generates a number in range [1..6]
            std::this_thread::sleep_for(std::chrono::milliseconds(rnd));
            tc.task_finished();
        });
    }

    // Wait for all the tasks to complete
    REQUIRE(tc.wait_for_all());

    // The WRITE task needs to be executed at the same position as it was enqueued.
    REQUIRE(results[write_pos] == write_pos);
    // All the READs enqueued before the WRITE should be executed before the WRITE
    for (int i = 0; i < write_pos; i++)
        REQUIRE(results[i] < write_pos);
    // All the READs enqueued after the WRITE should be executed after the WRITE
    for (int i = write_pos + 1; i < num_tasks; i++)
        REQUIRE(results[i] > write_pos);
}