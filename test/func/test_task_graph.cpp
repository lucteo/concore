#include <catch2/catch.hpp>
#include <concore/task_graph.hpp>
#include <concore/global_executor.hpp>

#include "test_common/task_countdown.hpp"
#include "test_common/task_utils.hpp"

TEST_CASE("one can define a simple linear chain of tasks", "[task_graph]") {
    CONCORE_PROFILING_FUNCTION();

    constexpr int num_tasks = 10;
    int counter{0};
    int res[num_tasks];

    task_countdown tc{num_tasks};

    // Create the tasks
    auto e = concore::global_executor;
    std::vector<concore::chained_task> tasks;
    tasks.reserve(num_tasks);
    for (int i = 0; i < num_tasks; i++) {
        tasks.emplace_back(
                [&, i]() {
                    res[i] = counter++;
                    tc.task_finished();
                },
                e);
    }
    // Add the dependencies
    for (int i = 1; i < num_tasks; i++) {
        concore::add_dependency(tasks[i - 1], tasks[i]);
    }

    // Start executing the chain from the first task
    e(tasks[0]);

    // Wait for all the tasks to complete
    REQUIRE(tc.wait_for_all());

    // Ensure the tasks are executed in order
    for (int i = 0; i < num_tasks; i++)
        REQUIRE(res[i] == i);
}

TEST_CASE("chained_task objects are copyable", "[task_graph]") {
    CONCORE_PROFILING_FUNCTION();

    task_countdown tc{2};

    auto e = concore::global_executor;
    concore::chained_task t1([&]() { tc.task_finished(); }, e);
    concore::chained_task t2([&]() { tc.task_finished(); }, e);

    concore::add_dependency(t1, t2);

    // make a copy of the first task
    concore::chained_task copy(t1);
    REQUIRE(&copy != &t1);

    // start executing from the copy
    e(copy);

    // both tasks are executed
    REQUIRE(tc.wait_for_all());
}

TEST_CASE("multiple links between the tasks (same direction) are fine", "[task_graph]") {
    CONCORE_PROFILING_FUNCTION();

    task_countdown tc{2};

    auto e = concore::global_executor;
    concore::chained_task t1([&]() { tc.task_finished(); }, e);
    concore::chained_task t2([&]() { tc.task_finished(); }, e);

    concore::add_dependency(t1, t2);
    concore::add_dependency(t1, t2);
    concore::add_dependency(t1, t2);
    concore::add_dependency(t1, t2);

    // Start with the first task, and wait for both tasks to execute
    e(t1);
    REQUIRE(tc.wait_for_all());
}

TEST_CASE("circular dependencies lead to tasks not being executed", "[task_graph]") {
    CONCORE_PROFILING_FUNCTION();

    task_countdown tc{4};
    bool executed[4] = {false};

    auto e = concore::global_executor;
    concore::chained_task t1(
            [&]() {
                executed[0] = true;
                tc.task_finished();
            },
            e);
    concore::chained_task t2(
            [&]() {
                executed[1] = true;
                tc.task_finished();
            },
            e);
    concore::chained_task t3(
            [&]() {
                executed[2] = true;
                tc.task_finished();
            },
            e);
    concore::chained_task t4(
            [&]() {
                executed[3] = true;
                tc.task_finished();
            },
            e);

    // t1 -> t2 <-> t3 -> t4

    concore::add_dependency(t1, t2);
    concore::add_dependency(t2, t3);
    concore::add_dependency(t3, t2);
    concore::add_dependency(t3, t4);

    // Start with the first task
    e(t1);
    // The tasks will never complete
    REQUIRE(!tc.wait_for_all(10ms));

    // Ensure that t2, t3 & t4 are not executed
    REQUIRE(!executed[1]);
    REQUIRE(!executed[2]);
    REQUIRE(!executed[3]);
}

TEST_CASE("a lot of continuations added to a chained_task", "[task_graph]") {
    CONCORE_PROFILING_FUNCTION();

    constexpr int num_tasks = 10;
    task_countdown tc{num_tasks};

    // Create the tasks
    auto e = concore::global_executor;
    std::vector<concore::chained_task> tasks;
    tasks.reserve(num_tasks);
    for (int i = 0; i < num_tasks; i++) {
        tasks.emplace_back([&]() { tc.task_finished(); }, e);
    }
    // Add the dependencies: first task to all the rest
    for (int i = 1; i < num_tasks; i++) {
        concore::add_dependency(tasks[0], tasks[i]);
    }

    // Start executing at the first task
    e(tasks[0]);

    // All tasks should execute
    REQUIRE(tc.wait_for_all());
}

TEST_CASE("a lot of predecessors added to a chained_task", "[task_graph]") {
    CONCORE_PROFILING_FUNCTION();

    constexpr int num_tasks = 10;
    task_countdown tc{num_tasks};
    bool executed[num_tasks] = {false};

    // Create the tasks
    auto e = concore::global_executor;
    std::vector<concore::chained_task> tasks;
    tasks.reserve(num_tasks);
    for (int i = 0; i < num_tasks; i++) {
        tasks.emplace_back(
                [&, i]() {
                    executed[i] = true;
                    tc.task_finished();
                },
                e);
    }
    // Add the dependencies: first task has as predecessors all the other tasks
    for (int i = 1; i < num_tasks; i++) {
        concore::add_dependency(tasks[i], tasks[0]);
    }

    // Execute about half of the tasks (not the first one though)
    for (int i = 1; i < num_tasks / 2; i++)
        e(tasks[i]);

    // After waiting a bit, the first task is still not executed
    std::this_thread::sleep_for(5ms);
    REQUIRE(!executed[0]);

    // Now, execute the other half (still don't execute the first task)
    for (int i = num_tasks / 2; i < num_tasks; i++)
        e(tasks[i]);

    // Now, all the tasks, including the first one should be executed
    REQUIRE(tc.wait_for_all());
    REQUIRE(executed[0]);
}

TEST_CASE("tree-like structure of chained_tasks", "[task_graph]") {
    CONCORE_PROFILING_FUNCTION();

    constexpr int num_tasks = 127; // needs to be 2^n-1
    task_countdown tc{num_tasks};

    // Create the tasks
    auto e = concore::global_executor;
    std::vector<concore::chained_task> tasks;
    tasks.reserve(num_tasks);
    for (int i = 0; i < num_tasks; i++) {
        tasks.emplace_back([&]() { tc.task_finished(); }, e);
    }
    // Add the dependencies; tree built similarly to the one build during heap sort
    for (int i = 0; i < num_tasks / 2; i++) {
        add_dependency(tasks[i], tasks[2 * i + 1]);
        add_dependency(tasks[i], tasks[2 * i + 2]);
    }
    // Start executing the first tasks
    e(tasks[0]);
    // Expect all the tasks in the graph to be executed
    REQUIRE(tc.wait_for_all());
}

TEST_CASE("reverse tree-like structure of chained_tasks", "[task_graph]") {
    CONCORE_PROFILING_FUNCTION();

    constexpr int num_tasks = 127; // needs to be 2^n-1
    task_countdown tc{num_tasks};

    // Create the tasks
    auto e = concore::global_executor;
    std::vector<concore::chained_task> tasks;
    tasks.reserve(num_tasks);
    for (int i = 0; i < num_tasks; i++) {
        tasks.emplace_back([&]() { tc.task_finished(); }, e);
    }
    // Add the dependencies; tree built similarly to the one build during heap sort
    for (int i = 0; i < num_tasks / 2; i++) {
        add_dependency(tasks[2 * i + 1], tasks[i]);
        add_dependency(tasks[2 * i + 2], tasks[i]);
    }
    // Execute the bottom of the tree
    for (int i = num_tasks / 2; i < num_tasks; i++)
        e(tasks[i]);

    // Expect all the tasks in the graph to be executed -- execution is propagated up
    REQUIRE(tc.wait_for_all());
}

TEST_CASE("hand-crafted graph of chained_tasks", "[task_graph]") {
    CONCORE_PROFILING_FUNCTION();

    constexpr int num_tasks = 18;
    task_countdown tc{num_tasks};

    // Create the tasks
    auto e = concore::global_executor;
    std::vector<concore::chained_task> tasks;
    tasks.reserve(num_tasks);
    for (int i = 0; i < num_tasks; i++) {
        tasks.emplace_back([&]() { tc.task_finished(); }, e);
    }
    //            /-->  6 -\
    //    *-> 1 -*--->  7 --*-> 14 ------*
    //    |       \-->  8 -------------* |
    //    |                             \|
    // 0 -*-> 2 --*-->  9 --*-> 15 ------*-> 17
    //    |\       \-> 10 -/            /|
    //    | \                          / |
    //    |  *---> 4 ----> 12 --------*  |
    //    *-> 3 -> 5 ----> 13 ---*-> 16 -*
    //          \----> 11 ------/
    //
    add_dependencies(tasks[0], {tasks[1], tasks[2], tasks[3], tasks[4]});
    add_dependencies(tasks[1], {tasks[6], tasks[7], tasks[8]});
    add_dependencies(tasks[2], {tasks[9], tasks[10]});
    add_dependencies(tasks[3], {tasks[5], tasks[11]});
    add_dependency(tasks[4], tasks[12]);
    add_dependency(tasks[5], tasks[13]);
    add_dependencies({tasks[6], tasks[7]}, tasks[14]);
    add_dependencies({tasks[9], tasks[10]}, tasks[15]);
    add_dependencies({tasks[11], tasks[13]}, tasks[16]);
    add_dependencies({tasks[14], tasks[8], tasks[15], tasks[12], tasks[16]}, tasks[17]);

    // Execute from the first task
    e(tasks[0]);

    // Expect all the tasks in the graph to be executed
    REQUIRE(tc.wait_for_all());
}

TEST_CASE("a chained_task can be reused after it was run", "[task_graph]") {
    CONCORE_PROFILING_FUNCTION();

    constexpr int num_tasks = 10;
    task_countdown tc{num_tasks};
    int cnt[num_tasks] = {0};

    // Create the tasks
    auto e = concore::global_executor;
    std::vector<concore::chained_task> tasks;
    tasks.reserve(num_tasks);
    for (int i = 0; i < num_tasks; i++) {
        tasks.emplace_back(
                [&, i]() {
                    cnt[i]++;
                    tc.task_finished();
                },
                e);
    }

    constexpr int num_runs = 5;
    for (int k = 0; k < num_runs; k++) {
        // Create a simple linear chain
        // We have to re-do the links every time
        for (int i = 1; i < num_tasks; i++)
            add_dependency(tasks[i - 1], tasks[i]);

        // Execute from the first task; expect all tasks to be executed
        e(tasks[0]);
        REQUIRE(tc.wait_for_all());
        tc.reset(num_tasks);
        std::this_thread::sleep_for(10ms); // ensure the task is truly finished
    }

    // Check that all the tasks were run several times
    for (int i = 0; i < num_tasks; i++)
        REQUIRE(cnt[i] == num_runs);

    // If we run the first task again, without setting the dependencies, only the first task is run
    tc.reset(1);
    e(tasks[0]);
    REQUIRE(tc.wait_for_all());
    // Wait a bit, so that other tasks have time to run
    std::this_thread::sleep_for(10ms);
    REQUIRE(cnt[0] == 1 + num_runs);
    for (int i = 1; i < num_tasks; i++)
        REQUIRE(cnt[i] == num_runs);
}

TEST_CASE("exceptions can occur in chained_task", "[task_graph]") {
    auto grp_wait = concore::task_group::create();
    auto finish_task = concore::task([]() {}, grp_wait);

    // Create two tasks
    bool task1_executed = false;
    bool task2_executed = false;
    auto t1 = concore::task([&]() {
        task1_executed = true;
        throw std::logic_error("err");
    });
    auto t2 = concore::task([&]() {
        task2_executed = true;
        concore::global_executor(std::move(finish_task));
    });
    // Chain them in a task_graph
    auto ct1 = concore::chained_task(std::move(t1), concore::global_executor);
    auto ct2 = concore::chained_task(std::move(t2), concore::global_executor);
    concore::add_dependency(ct1, ct2);
    // Start executing the first one
    ct1();
    // Wait for the tasks to complete
    REQUIRE(bounded_wait(grp_wait));
    // Ensure both tasks are run
    REQUIRE(task1_executed);
    REQUIRE(task2_executed);
}

TEST_CASE("exceptions in task graphs are caught by the group", "[task_graph]") {
    auto grp_wait = concore::task_group::create();
    auto finish_task = concore::task([]() {}, grp_wait);

    // Set up a group for checking exceptions
    int ex_count = 0;
    auto grp_ex = concore::task_group::create();
    grp_ex.set_exception_handler([&](std::exception_ptr) { ex_count++; });

    // Create two tasks
    bool task1_executed = false;
    bool task2_executed = false;
    auto t1 = concore::task(
            [&]() {
                task1_executed = true;
                throw std::logic_error("err");
            },
            grp_ex);
    auto t2 = concore::task(
            [&]() {
                task2_executed = true;
                concore::global_executor(std::move(finish_task));
                throw std::logic_error("err");
            },
            grp_ex);
    // Chain them in a task_graph
    auto ct1 = concore::chained_task(std::move(t1), concore::global_executor);
    auto ct2 = concore::chained_task(std::move(t2), concore::global_executor);
    concore::add_dependency(ct1, ct2);
    // Start executing the first one
    ct1();
    // Wait for the tasks to complete
    REQUIRE(bounded_wait(grp_wait));
    // Ensure both tasks are run
    REQUIRE(task1_executed);
    REQUIRE(task2_executed);
    // Ensure that we caught both exceptions
    REQUIRE(ex_count == 2);
}

TEST_CASE("chained_task can be created without an executor", "[task_graph]") {
    auto grp_wait = concore::task_group::create();
    auto finish_task = concore::task([]() {}, grp_wait);

    // Create two tasks
    bool task1_executed = false;
    bool task2_executed = false;
    auto t1 = concore::task([&]() { task1_executed = true; });
    auto t2 = concore::task([&]() {
        task2_executed = true;
        concore::global_executor(std::move(finish_task));
    });
    // Chain them in a task_graph
    auto ct1 = concore::chained_task(std::move(t1));
    auto ct2 = concore::chained_task(std::move(t2));
    concore::add_dependency(ct1, ct2);
    // Start executing the first one
    ct1();
    // Wait for the tasks to complete
    REQUIRE(bounded_wait(grp_wait));
    // Ensure both tasks are run
    REQUIRE(task1_executed);
    REQUIRE(task2_executed);
}

struct throwing_executor {
    void operator()(concore::task&& t) {
        concore::spawn(std::move(t));
        throw std::logic_error("err");
    }
};

TEST_CASE("chained_task works (somehow) with an executor that throws", "[task_graph]") {
    auto grp_wait = concore::task_group::create();
    auto finish_task = concore::task([]() {}, grp_wait);

    // Crete the tasks
    bool executed[5] = {false, false, false, false, false};
    auto t1 = concore::chained_task([&]() { executed[0] = true; }, throwing_executor{});
    auto t2 = concore::chained_task([&]() { executed[1] = true; }, throwing_executor{});
    auto t3 = concore::chained_task([&]() { executed[2] = true; }, throwing_executor{});
    auto t4 = concore::chained_task([&]() { executed[3] = true; }, throwing_executor{});
    auto t5 = concore::chained_task(
            [&]() {
                executed[4] = true;
                concore::global_executor(std::move(finish_task));
            },
            throwing_executor{});

    // Add the dependencies
    concore::add_dependencies(t1, {t2, t3, t4});
    concore::add_dependencies({t2, t3, t4}, t5);

    // Set up the exception handlers
    std::atomic<int> num_ex = 0;
    concore::except_fun_t ex_handler = [&](std::exception_ptr) { num_ex++; };
    t1.set_exception_handler(ex_handler);
    t2.set_exception_handler(ex_handler);
    t3.set_exception_handler(ex_handler);
    t4.set_exception_handler(ex_handler);
    t5.set_exception_handler(ex_handler);

    // Start executing the graph
    t1();
    // Wait for the tasks to complete
    REQUIRE(bounded_wait(grp_wait));
    // Ensure all tasks are executed
    REQUIRE(executed[0]);
    REQUIRE(executed[1]);
    REQUIRE(executed[2]);
    REQUIRE(executed[3]);
    REQUIRE(executed[4]);

    // Ensure that our exception handler is called
    REQUIRE(num_ex.load() == 4);
}
