#include <catch2/catch.hpp>
#include <concore/global_executor.hpp>
#include <concore/inline_executor.hpp>
#include <concore/spawn.hpp>
#include <concore/execution.hpp>

#include <atomic>
#include <thread>

TEST_CASE("inline_executor also calls the finish ftor of a task", "[task]") {
    bool main_fun_called{false};
    bool cont_fun_called{false};

    auto f = [&] { main_fun_called = true; };
    auto cont = [&](std::exception_ptr ex) {
        REQUIRE(!ex);
        cont_fun_called = true;
    };

    // Execute the task
    concore::task t{f, {}, cont};
    concore::execute(concore::inline_executor{}, std::move(t));

    // Ensure that both ftors are called
    REQUIRE(main_fun_called);
    REQUIRE(cont_fun_called);
}

TEST_CASE("global_executor also calls the finish ftor of a task", "[task]") {
    bool main_fun_called{false};
    bool cont_fun_called{false};

    auto f = [&] { main_fun_called = true; };
    auto cont = [&](std::exception_ptr ex) {
        REQUIRE(!ex);
        cont_fun_called = true;
    };

    // Execute the task
    concore::task_group grp = concore::task_group::create();
    concore::task t{f, grp, cont};
    concore::execute(concore::global_executor{}, std::move(t));
    concore::wait(grp);

    // Ensure that both ftors are called
    REQUIRE(main_fun_called);
    REQUIRE(cont_fun_called);
}

TEST_CASE("finish ftor of a task is called on an exception", "[task]") {
    struct my_exception : std::runtime_error {
        my_exception(int val)
            : std::runtime_error("test")
            , val_(val) {}
        int val_{0};
    };

    bool cont_fun_called{false};
    auto f = [&] { throw my_exception(10); };
    auto cont = [&](std::exception_ptr ex) {
        REQUIRE(ex);
        int received_val{0};
        try {
            std::rethrow_exception(ex);
        } catch (my_exception& ex) {
            received_val = ex.val_;
        } catch (...) {
            REQUIRE(false);
        }
        REQUIRE(received_val == 10);
        cont_fun_called = true;
    };

    // Execute the task
    concore::task t{f, {}, cont};
    try {
        concore::execute(concore::inline_executor{}, std::move(t));
    } catch (...) {
    }

    // Ensure that the finish function was called
    REQUIRE(cont_fun_called);
}

TEST_CASE("continuation function can be inspected by the running task", "[task]") {
    int cont_called{0};

    auto cont = [&](std::exception_ptr ex) {
        REQUIRE(!ex);
        cont_called++;
    };
    auto expect_no_cont = [&] {
        auto* cur_task = concore::task::current_task();
        REQUIRE(cur_task != nullptr);
        auto cont = cur_task->get_continuation();
        REQUIRE(!cont);
    };
    auto expect_cont = [&] {
        auto* cur_task = concore::task::current_task();
        REQUIRE(cur_task != nullptr);
        auto cont = cur_task->get_continuation();
        REQUIRE(cont);
        // Call the continuation function
        cont(std::exception_ptr{});
    };

    // Execute a task without continuation
    cont_called = 0;
    concore::task t1{expect_no_cont};
    concore::execute(concore::inline_executor{}, std::move(t1));
    REQUIRE(cont_called == 0);

    // Execute a task with continuation
    cont_called = 0;
    concore::task t2{expect_cont, {}, cont};
    concore::execute(concore::inline_executor{}, std::move(t2));
    REQUIRE(cont_called == 2);
}

TEST_CASE("continuation function can be changed by the running task", "[task]") {
    bool cont1_called{false};
    bool cont2_called{false};

    concore::task_continuation_function cont1 = [&](std::exception_ptr ex) {
        REQUIRE(!ex);
        cont1_called = true;
    };
    concore::task_continuation_function cont2 = [&](std::exception_ptr ex) {
        REQUIRE(!ex);
        cont2_called = true;
    };
    auto task_fun = [&] {
        auto* cur_task = concore::task::current_task();
        REQUIRE(cur_task != nullptr);
        auto cont = cur_task->get_continuation();
        REQUIRE(cont);
        cur_task->set_continuation(cont2);
    };

    // Create and execute a task with the first continuation
    concore::task t{task_fun, {}, cont1};
    concore::execute(concore::inline_executor{}, std::move(t));

    // Ensure that the second continuation is called (i.e., continuation was switched)
    REQUIRE_FALSE(cont1_called);
    REQUIRE(cont2_called);
}

TEST_CASE("tasks can be chained with continuations (while task is executing)", "[task]") {
    bool cont_called{false};
    int counter{0};

    auto initial_cont = [&](std::exception_ptr ex) {
        REQUIRE(!ex);
        cont_called = true;
    };

    concore::task_group grp = concore::task_group::create();

    int num_remaining_{9};
    concore::task_function fun = [&]() {
        if (num_remaining_-- > 0) {
            // Introduce another task in the chain
            auto* cur_task = concore::task::current_task();
            REQUIRE(cur_task != nullptr);
            auto cur_cont = cur_task->get_continuation();

            auto new_cont = [&fun, grp, cur_cont](std::exception_ptr ex) {
                concore::task t{fun, grp, cur_cont};
                concore::execute(concore::global_executor{}, std::move(t));
            };
            cur_task->set_continuation(std::move(new_cont));
        }

        // Do the execution of this task
        counter++;
    };

    // Start the execution chain
    // Each task will create another task
    concore::task start_task{fun, grp, initial_cont};
    concore::execute(concore::global_executor{}, std::move(start_task));
    concore::wait(grp);

    // Ensure that we executed the whole chain
    REQUIRE(counter == 10);
    REQUIRE(cont_called);
}
