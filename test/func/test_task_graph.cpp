#include <catch2/catch.hpp>
#include <concore/task_graph.hpp>
#include <concore/global_executor.hpp>

#include "test_common/task_countdown.hpp"

TEST_CASE("one can define a simple linear chain of tasks") {
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

TEST_CASE("chained_task objects are copyable") {
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

TEST_CASE("multiple links between the tasks (same direction) are fine") {
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

TEST_CASE("circular dependencies lead to tasks not being executed") {
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

TEST_CASE("a lot of continuations added to a chained_task") {
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

TEST_CASE("a lot of predecessors added to a chained_task") {
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

TEST_CASE("tree-like structure of chained_tasks") {
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

TEST_CASE("reverse tree-like structure of chained_tasks") {
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

TEST_CASE("hand-crafted graph of chained_tasks") {
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

TEST_CASE("a chained_task can be reused after it was run") {
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
    }

    // Check that all the tasks were run several times
    for (int i = 0; i < num_tasks; i++)
        REQUIRE(cnt[i] == num_runs);

    // If we run the first task again, without setting the dependencies, only the first task is run
    e(tasks[0]);
    REQUIRE(!tc.wait_for_all(10ms));
}
