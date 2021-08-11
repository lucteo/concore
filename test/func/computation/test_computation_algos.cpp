#include <catch2/catch.hpp>

#include <concore/computation/computation.hpp>
#include <concore/computation/just_value.hpp>
#include <concore/computation/from_task.hpp>
#include <concore/computation/transform.hpp>
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

TEST_CASE("transform with void(void) functor", "[computation") {
    bool executed{false};
    auto f = [&executed] { executed = true; };

    auto c0 = concore::computation::just_void();
    auto c = concore::computation::transform(c0, std::move(f));
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    REQUIRE(called);
    REQUIRE(executed);
}

TEST_CASE("transform with void(int) functor", "[computation") {
    bool executed{false};
    auto f = [&executed](int x) {
        REQUIRE(x == 10);
        executed = true;
    };

    auto c0 = concore::computation::just_value(10);
    auto c = concore::computation::transform(c0, std::move(f));
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    REQUIRE(called);
    REQUIRE(executed);
}

TEST_CASE("transform with int(void) functor", "[computation") {
    bool executed{false};
    auto f = [&executed]() -> int {
        executed = true;
        return 10;
    };

    auto c0 = concore::computation::just_void();
    auto c = concore::computation::transform(c0, std::move(f));
    ensure_computation<decltype(c), int>();

    int res{0};
    concore::computation::run_with(c, test_value_receiver<int>{&res});
    REQUIRE(res == 10);
    REQUIRE(executed);
}

TEST_CASE("transform with int(int) functor", "[computation") {
    bool executed{false};
    auto f = [&executed](int x) -> int {
        executed = true;
        return x * x;
    };

    auto c0 = concore::computation::just_value(10);
    auto c = concore::computation::transform(c0, std::move(f));
    ensure_computation<decltype(c), int>();

    int res{0};
    concore::computation::run_with(c, test_value_receiver<int>{&res});
    REQUIRE(res == 100);
    REQUIRE(executed);
}

TEST_CASE("transform on a thread_pool", "[computation") {
    concore::static_thread_pool pool{1};

    auto c0 = concore::computation::from_task(concore::task{[] {}}, pool.executor());
    auto f = []() -> int { return 10; };
    auto c = concore::computation::transform(c0, std::move(f));
    ensure_computation<decltype(c), int>();

    int res{0};
    concore::computation::run_with(c, test_value_receiver<int>{&res});
    pool.wait();
    REQUIRE(res == 10);
}

TEST_CASE("transform calls set_error if the functor throws", "[computation") {
    bool executed{false};
    auto f = [&executed](int x) {
        executed = true;
        throw std::logic_error("error");
    };

    auto c0 = concore::computation::just_value(10);
    auto c = concore::computation::transform(c0, std::move(f));
    ensure_computation<decltype(c), void>();

    bool recv_called{false};
    concore::computation::run_with(c, test_error_receiver{&recv_called});
    REQUIRE(recv_called);
    REQUIRE(executed);
}

TEST_CASE("transform forwards errors", "[computation") {
    bool executed{false};
    auto f = [&executed]() { executed = true; };

    auto c0 = concore::computation::from_task(concore::task{[] { throw std::logic_error("err"); }});
    auto c = concore::computation::transform(c0, std::move(f));
    ensure_computation<decltype(c), void>();

    bool recv_called{false};
    concore::computation::run_with(c, test_error_receiver{&recv_called});
    REQUIRE(recv_called);
    REQUIRE_FALSE(executed);
}

TEST_CASE("transform forwards cancellation", "[computation") {
    bool executed{false};
    auto f = [&executed]() { executed = true; };

    auto grp = concore::task_group::create();
    grp.cancel();

    auto c0 = concore::computation::from_task(concore::task{[] {}, grp});
    auto c = concore::computation::transform(c0, std::move(f));
    ensure_computation<decltype(c), void>();

    bool recv_called{false};
    concore::computation::run_with(c, test_done_receiver{&recv_called});
    REQUIRE(recv_called);
    REQUIRE_FALSE(executed);
}
