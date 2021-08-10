#include <catch2/catch.hpp>

#include <concore/computation/computation.hpp>
#include <concore/computation/just_value.hpp>
#include <concore/computation/from_task.hpp>
#include <concore/as_receiver.hpp>
#include <concore/inline_executor.hpp>
#include <concore/thread_pool.hpp>
#include <test_common/throwing_executor.hpp>

#include <string>
#include <atomic>

template <typename C, typename R>
void ensure_computation() {
    static_assert(concore::computation::computation<C>, "Given type is not a computation");
    static_assert(std::is_same_v<typename C::value_type, R>, "Invalid value_type in computation");
}

template <typename T>
struct test_value_receiver {
    T* res;
    void set_value(T v) { *res = v; }
    void set_done() noexcept { FAIL("set_done() called"); }
    void set_error(std::exception_ptr) noexcept { FAIL("set_error() called"); }
};

struct test_void_receiver {
    bool* called;
    void set_value() { *called = true; }
    void set_done() noexcept { FAIL("set_done() called"); }
    void set_error(std::exception_ptr) noexcept { FAIL("set_error() called"); }
};

struct test_error_receiver {
    bool* called;
    void set_value() { FAIL("set_value() called"); }
    void set_done() noexcept { FAIL("set_done() called"); }
    void set_error(std::exception_ptr) noexcept { *called = true; }
};

struct test_done_receiver {
    bool* called;
    void set_value() { FAIL("set_value() called"); }
    void set_done() noexcept { *called = true; }
    void set_error(std::exception_ptr) noexcept { FAIL("set_error() called"); }
};

TEST_CASE("just_value(int) is a computation that yields the specified value", "[computation]") {
    auto c = concore::computation::just_value(10);
    ensure_computation<decltype(c), int>();

    int res{0};
    concore::computation::run_with(c, test_value_receiver<int>{&res});
    REQUIRE(res == 10);
}

TEST_CASE("just_value(string) is a computation that yields the specified value", "[computation]") {
    auto c = concore::computation::just_value(std::string{"Hello, world!"});
    ensure_computation<decltype(c), std::string>();

    std::string res;
    concore::computation::run_with(c, test_value_receiver<std::string>{&res});
    REQUIRE(res == "Hello, world!");
}

TEST_CASE("just_value() is a computation that yields nothing", "[computation]") {
    auto c = concore::computation::just_value();
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    REQUIRE(called);
}

TEST_CASE("just_void() is a computation that yields nothing", "[computation]") {
    auto c = concore::computation::just_void();
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    REQUIRE(called);
}

TEST_CASE("just_task with a no-error task will call set_value", "[computation]") {
    concore::task t{[] {}};

    auto c = concore::computation::from_task(std::move(t));
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    REQUIRE(called);
}

TEST_CASE("just_task with a task returning an error will call set_error", "[computation]") {
    concore::task t{[] { throw std::logic_error("error"); }};

    auto c = concore::computation::from_task(std::move(t));
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_error_receiver{&called});
    REQUIRE(called);
}

TEST_CASE("just_task with a task on a cancelled group will call set_done", "[computation]") {
    auto grp = concore::task_group::create();
    grp.cancel();
    concore::task t{[] {}, std::move(grp)};

    auto c = concore::computation::from_task(std::move(t));
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_done_receiver{&called});
    REQUIRE(called);
}

TEST_CASE("just_task on a task with continuation will ensure the task continuation is called "
          "before the receiver",
        "[computation]") {
    std::atomic<int> counter{0};
    auto f = [&counter] { REQUIRE(counter++ == 0); };
    auto cont = [&counter](std::exception_ptr ex) {
        REQUIRE(!ex);
        REQUIRE(counter++ == 1);
    };
    auto recv_fun = [&counter] { REQUIRE(counter++ == 2); };

    concore::task t{std::move(f), {}, std::move(cont)};

    auto c = concore::computation::from_task(std::move(t));
    ensure_computation<decltype(c), void>();

    auto recv = concore::as_receiver<decltype(recv_fun)>(std::move(recv_fun));
    concore::computation::run_with(c, recv);
    REQUIRE(counter.load() == 3);
}

TEST_CASE("just_task can run with inline_executor", "[computation]") {
    bool executed{false};
    auto f = [&executed] { executed = true; };
    concore::task t{std::move(f)};

    auto c = concore::computation::from_task(std::move(t), concore::inline_executor{});
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    REQUIRE(called);
    REQUIRE(executed);
}

TEST_CASE("just_task can run with thread_pool executor", "[computation]") {
    concore::static_thread_pool pool{1};
    auto ex = pool.executor();

    bool executed{false};
    auto f = [&executed, &ex] {
        REQUIRE(ex.running_in_this_thread());
        executed = true;
    };
    concore::task t{std::move(f)};

    auto c = concore::computation::from_task(std::move(t), ex);
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    pool.wait();
    REQUIRE(called);
    REQUIRE(executed);
}

TEST_CASE("just_task can run with a stopped thread_pool executor, calling set_done()",
        "[computation]") {
    concore::static_thread_pool pool{1};
    auto ex = pool.executor();
    pool.stop();

    bool executed{false};
    auto f = [&executed] { executed = true; };
    concore::task t{std::move(f)};

    auto c = concore::computation::from_task(std::move(t), ex);
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_done_receiver{&called});
    pool.wait();
    REQUIRE(called);
    REQUIRE_FALSE(executed);
}

TEST_CASE("just_task can run with a throwing executor, calling set_done()", "[computation]") {
    bool executed{false};
    auto f = [&executed] { executed = true; };
    concore::task t{std::move(f)};

    auto c = concore::computation::from_task(std::move(t), throwing_executor{});
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_error_receiver{&called});
    REQUIRE(called);
    REQUIRE_FALSE(executed);
}
