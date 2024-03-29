#include <catch2/catch.hpp>

#include <concore/computation/computation.hpp>
#include <concore/computation/just_value.hpp>
#include <concore/computation/from_function.hpp>
#include <concore/computation/from_task.hpp>
#include <concore/computation/transform.hpp>
#include <concore/computation/bind.hpp>
#include <concore/computation/on.hpp>
#include <concore/computation/wait.hpp>
#include <concore/computation/to_task.hpp>
#include <concore/computation/run.hpp>
#include <concore/as_receiver.hpp>
#include <concore/inline_executor.hpp>
#include <concore/spawn.hpp>
#include <concore/thread_pool.hpp>
#include <test_common/throwing_executor.hpp>

#include <string>
#include <atomic>

using namespace concore::computation;

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
    template <typename... Ts>
    void set_value(Ts...) {
        FAIL("set_value() called");
    }
    void set_done() noexcept { FAIL("set_done() called"); }
    void set_error(std::exception_ptr) noexcept { *called = true; }
};

struct test_done_receiver {
    bool* called;
    template <typename... Ts>
    void set_value(Ts...) {
        FAIL("set_value() called");
    }
    void set_done() noexcept { *called = true; }
    void set_error(std::exception_ptr) noexcept { FAIL("set_error() called"); }
};

TEST_CASE("just_value(int) is a computation that yields the specified value", "[computation]") {
    auto c = concore::computation::just_value(10);
    ensure_computation<decltype(c), int>();

    int res{0};
    concore::computation::run_with(c, test_value_receiver<int>{&res});
    CHECK(res == 10);
}

TEST_CASE("just_value(string) is a computation that yields the specified value", "[computation]") {
    auto c = concore::computation::just_value(std::string{"Hello, world!"});
    ensure_computation<decltype(c), std::string>();

    std::string res;
    concore::computation::run_with(c, test_value_receiver<std::string>{&res});
    CHECK(res == "Hello, world!");
}

TEST_CASE("just_value() is a computation that yields nothing", "[computation]") {
    auto c = concore::computation::just_value();
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    CHECK(called);
}

TEST_CASE("just_void() is a computation that yields nothing", "[computation]") {
    auto c = concore::computation::just_void();
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    CHECK(called);
}

TEST_CASE("from_function with a function returning a value", "[computation]") {
    auto c = from_function([] { return 3; });
    ensure_computation<decltype(c), int>();

    int res{0};
    concore::computation::run_with(c, test_value_receiver<int>{&res});
    CHECK(res == 3);
}

TEST_CASE("from_function with a function returning void", "[computation]") {
    bool ftor_called{false};
    auto c = from_function([&ftor_called] { ftor_called = true; });
    ensure_computation<decltype(c), void>();

    bool recv_called{false};
    concore::computation::run_with(c, test_void_receiver{&recv_called});
    CHECK(recv_called);
    CHECK(ftor_called);
}

TEST_CASE("from_function with a throwing function reports an error", "[computation]") {
    auto c = from_function([] { throw std::logic_error("err"); });
    ensure_computation<decltype(c), void>();

    bool recv_called{false};
    concore::computation::run_with(c, test_error_receiver{&recv_called});
    CHECK(recv_called);
}

TEST_CASE("from_task with a no-error task will call set_value", "[computation]") {
    concore::task t{[] {}};

    auto c = concore::computation::from_task(std::move(t));
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    CHECK(called);
}

TEST_CASE("from_task with a task returning an error will call set_error", "[computation]") {
    concore::task t{[] { throw std::logic_error("error"); }};

    auto c = concore::computation::from_task(std::move(t));
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_error_receiver{&called});
    CHECK(called);
}

TEST_CASE("from_task with a task on a cancelled group will call set_done", "[computation]") {
    auto grp = concore::task_group::create();
    grp.cancel();
    concore::task t{[] {}, std::move(grp)};

    auto c = concore::computation::from_task(std::move(t));
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_done_receiver{&called});
    CHECK(called);
}

TEST_CASE("from_task on a task with continuation will ensure the task continuation is called "
          "before the receiver",
        "[computation]") {
    std::atomic<int> counter{0};
    auto f = [&counter] { CHECK(counter++ == 0); };
    auto cont = [&counter](std::exception_ptr ex) {
        CHECK(!ex);
        CHECK(counter++ == 1);
    };
    auto recv_fun = [&counter] { CHECK(counter++ == 2); };

    concore::task t{std::move(f), {}, std::move(cont)};

    auto c = concore::computation::from_task(std::move(t));
    ensure_computation<decltype(c), void>();

    auto recv = concore::as_receiver<decltype(recv_fun)>(std::move(recv_fun));
    concore::computation::run_with(c, recv);
    CHECK(counter.load() == 3);
}

TEST_CASE("from_task can run with inline_executor", "[computation]") {
    bool executed{false};
    auto f = [&executed] { executed = true; };
    concore::task t{std::move(f)};

    auto c = concore::computation::from_task(std::move(t), concore::inline_executor{});
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    CHECK(called);
    CHECK(executed);
}

TEST_CASE("from_task can run with thread_pool executor", "[computation]") {
    concore::static_thread_pool pool{1};
    auto ex = pool.executor();

    bool executed{false};
    auto f = [&executed, &ex] {
        CHECK(ex.running_in_this_thread());
        executed = true;
    };
    concore::task t{std::move(f)};

    auto c = concore::computation::from_task(std::move(t), ex);
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    pool.wait();
    CHECK(called);
    CHECK(executed);
}

TEST_CASE("from_task can run with a stopped thread_pool executor, calling set_done()",
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
    CHECK(called);
    CHECK_FALSE(executed);
}

TEST_CASE("from_task can run with a throwing executor, calling set_done()", "[computation]") {
    bool executed{false};
    auto f = [&executed] { executed = true; };
    concore::task t{std::move(f)};

    auto c = concore::computation::from_task(std::move(t), throwing_executor{});
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_error_receiver{&called});
    CHECK(called);
    CHECK_FALSE(executed);
}

TEST_CASE("transform with void(void) functor", "[computation]") {
    bool executed{false};
    auto f = [&executed] { executed = true; };

    auto c0 = concore::computation::just_void();
    auto c = concore::computation::transform(c0, std::move(f));
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    CHECK(called);
    CHECK(executed);
}

TEST_CASE("transform with void(int) functor", "[computation]") {
    bool executed{false};
    auto f = [&executed](int x) {
        CHECK(x == 10);
        executed = true;
    };

    auto c0 = concore::computation::just_value(10);
    auto c = concore::computation::transform(c0, std::move(f));
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    CHECK(called);
    CHECK(executed);
}

TEST_CASE("transform with int(void) functor", "[computation]") {
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
    CHECK(res == 10);
    CHECK(executed);
}

TEST_CASE("transform with int(int) functor", "[computation]") {
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
    CHECK(res == 100);
    CHECK(executed);
}

TEST_CASE("transform on a thread_pool", "[computation]") {
    concore::static_thread_pool pool{1};

    auto c0 = concore::computation::from_task(concore::task{[] {}}, pool.executor());
    auto f = []() -> int { return 10; };
    auto c = concore::computation::transform(c0, std::move(f));
    ensure_computation<decltype(c), int>();

    int res{0};
    concore::computation::run_with(c, test_value_receiver<int>{&res});
    pool.wait();
    CHECK(res == 10);
}

TEST_CASE("transform calls set_error if the functor throws", "[computation]") {
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
    CHECK(recv_called);
    CHECK(executed);
}

TEST_CASE("transform forwards errors", "[computation]") {
    bool executed{false};
    auto f = [&executed]() { executed = true; };

    auto c0 = concore::computation::from_task(concore::task{[] { throw std::logic_error("err"); }});
    auto c = concore::computation::transform(c0, std::move(f));
    ensure_computation<decltype(c), void>();

    bool recv_called{false};
    concore::computation::run_with(c, test_error_receiver{&recv_called});
    CHECK(recv_called);
    CHECK_FALSE(executed);
}

TEST_CASE("transform forwards cancellation", "[computation]") {
    bool executed{false};
    auto f = [&executed]() { executed = true; };

    auto grp = concore::task_group::create();
    grp.cancel();

    auto c0 = concore::computation::from_task(concore::task{[] {}, grp});
    auto c = concore::computation::transform(c0, std::move(f));
    ensure_computation<decltype(c), void>();

    bool recv_called{false};
    concore::computation::run_with(c, test_done_receiver{&recv_called});
    CHECK(recv_called);
    CHECK_FALSE(executed);
}

template <typename PrevComp, typename F>
auto transform_with_bind(PrevComp c, F f) {
    using interim_type = typename PrevComp::value_type;
    auto chainFun = [f](interim_type val) { return concore::computation::just_value(f(val)); };
    return concore::computation::bind(std::move(c), std::move(chainFun));
}

TEST_CASE("bind simulating an int transform", "[computation]") {
    auto f = [](int x) -> int { return x * x; };

    auto c0 = concore::computation::just_value(10);
    auto c = transform_with_bind(c0, std::move(f));
    ensure_computation<decltype(c), int>();

    int res{0};
    concore::computation::run_with(c, test_value_receiver<int>{&res});
    CHECK(res == 100);
}

TEST_CASE("bind that chains a comp returning int with one returning void", "[computation]") {
    auto c0 = concore::computation::just_value(10);
    auto f = [](int x) {
        CHECK(x == 10);
        return concore::computation::just_void();
    };
    auto c = concore::computation::bind(c0, std::move(f));
    ensure_computation<decltype(c), void>();

    bool recv_caled{false};
    concore::computation::run_with(c, test_void_receiver{&recv_caled});
    CHECK(recv_caled);
}

TEST_CASE("bind doesn't call ftor in case of error", "[computation]") {
    auto f = [] {
        FAIL_CHECK("ftor was not expected to be called");
        return just_void();
    };

    auto c0 = just_void();
    auto c1 = transform(c0, [] { throw std::logic_error("err"); });
    auto c = bind(c1, std::move(f));
    ensure_computation<decltype(c), void>();

    bool recv_caled{false};
    concore::computation::run_with(c, test_error_receiver{&recv_caled});
    CHECK(recv_caled);
}

TEST_CASE("bind doesn't call ftor in case of cancellation", "[computation]") {
    auto f = [] {
        FAIL_CHECK("ftor was not expected to be called");
        return just_void();
    };

    auto grp = concore::task_group::create();
    grp.cancel();
    auto t = concore::task{[] {}, grp};

    auto c0 = from_task(std::move(t));
    auto c = bind(c0, std::move(f));
    ensure_computation<decltype(c), void>();

    bool recv_caled{false};
    concore::computation::run_with(c, test_done_receiver{&recv_caled});
    CHECK(recv_caled);
}

TEST_CASE("bind calls set_error if the ftor throws", "[computation]") {
    auto f = [] {
        throw std::logic_error("err");
        return just_void();
    };

    auto c0 = just_void();
    auto c = bind(c0, std::move(f));
    ensure_computation<decltype(c), void>();

    bool recv_caled{false};
    concore::computation::run_with(c, test_error_receiver{&recv_caled});
    CHECK(recv_caled);
}

TEST_CASE("bind_error calls the ftor when the prev computation reports error", "[computation]") {
    bool ftor_called{false};
    auto f = [&ftor_called](std::exception_ptr) {
        ftor_called = true;
        return just_void();
    };

    auto c0 = just_void();
    auto c1 = transform(c0, [] { throw std::logic_error("err"); });
    auto c = bind_error(c1, std::move(f));
    ensure_computation<decltype(c), void>();

    bool recv_caled{false};
    concore::computation::run_with(c, test_void_receiver{&recv_caled});
    CHECK(recv_caled);
    CHECK(ftor_called);
}

TEST_CASE("bind_error with error, returning ints", "[computation]") {
    bool ftor_called{false};
    auto f = [&ftor_called](std::exception_ptr) {
        ftor_called = true;
        return just_value(10);
    };

    auto c0 = just_void();
    auto c1 = transform(c0, [] {
        throw std::logic_error("err");
        return 3;
    });
    auto c = bind_error(c1, std::move(f));
    ensure_computation<decltype(c), int>();

    int res{0};
    concore::computation::run_with(c, test_value_receiver<int>{&res});
    CHECK(res == 10);
}

TEST_CASE("bind_error doesn't call the ftor when the prev computation succeeds", "[computation]") {
    bool ftor_called{false};
    auto f = [&ftor_called](std::exception_ptr) {
        ftor_called = true;
        return just_value(10);
    };

    auto c = bind_error(just_value(3), std::move(f));
    ensure_computation<decltype(c), int>();

    int res{0};
    concore::computation::run_with(c, test_value_receiver<int>{&res});
    CHECK(res == 3);
}

TEST_CASE("bind_error doesn't call the ftor when the prev computation was cancelled",
        "[computation]") {
    bool ftor_called{false};
    auto f = [&ftor_called](std::exception_ptr) {
        ftor_called = true;
        return just_value(10);
    };

    auto grp = concore::task_group::create();
    grp.cancel();
    concore::task t{[] {}, std::move(grp)};

    auto c0 = from_task(std::move(t));
    auto c1 = transform(std::move(c0), [] { return 3; });
    auto c = bind_error(c1, std::move(f));
    ensure_computation<decltype(c), int>();

    bool recv_caled{false};
    concore::computation::run_with(c, test_done_receiver{&recv_caled});
    CHECK(recv_caled);
    CHECK_FALSE(ftor_called);
}

TEST_CASE("bind_error calls set_error if the ftor throws", "[computation]") {
    bool ftor_called{false};
    auto f = [&ftor_called](std::exception_ptr) {
        ftor_called = true;
        throw std::logic_error("err1");
        return just_void();
    };

    auto c0 = just_void();
    auto c1 = transform(c0, [] { throw std::logic_error("err2"); });
    auto c = bind_error(c1, std::move(f));
    ensure_computation<decltype(c), void>();

    bool recv_caled{false};
    concore::computation::run_with(c, test_error_receiver{&recv_caled});
    CHECK(recv_caled);
    CHECK(ftor_called);
}

TEST_CASE("on can change the thread", "[computation]") {
    concore::static_thread_pool pool{1};
    auto ex = pool.executor();

    auto c = on(just_value(10), ex);

    bool ftor_called{false};
    auto c_after = transform(c, [&ftor_called, &ex](int x) {
        ftor_called = true;
        CHECK(x == 10);
        CHECK(ex.running_in_this_thread());
        return x * x;
    });
    int res{0};
    concore::computation::run_with(c_after, test_value_receiver<int>{&res});
    pool.wait();
    CHECK(res == 100);
    CHECK(ftor_called);
}

TEST_CASE("on changing a thread with a void computation", "[computation]") {
    concore::static_thread_pool pool{1};
    auto ex = pool.executor();

    auto c = on(just_void(), ex);

    bool ftor_called{false};
    auto c_after = transform(c, [&ftor_called, &ex]() {
        ftor_called = true;
        CHECK(ex.running_in_this_thread());
    });
    bool recv_called{false};
    concore::computation::run_with(c_after, test_void_receiver{&recv_called});
    pool.wait();
    CHECK(recv_called);
    CHECK(ftor_called);
}

TEST_CASE("on applied to a computation that yields error will forward the error", "[computation]") {
    auto c0 = from_function([] { throw std::logic_error("err"); });
    auto c = on(c0, concore::inline_executor{});

    bool recv_called{false};
    concore::computation::run_with(c, test_error_receiver{&recv_called});
    CHECK(recv_called);
}

TEST_CASE("on applied to a computation that is cancelled  will forward the cancellation",
        "[computation]") {
    auto grp = concore::task_group::create();
    grp.cancel();
    auto c0 = from_task(concore::task{[] {}, grp});
    auto c = on(c0, concore::inline_executor{});

    bool recv_called{false};
    concore::computation::run_with(c, test_done_receiver{&recv_called});
    CHECK(recv_called);
}

TEST_CASE("on with an executor that yields an error", "[computation]") {
    auto c = on(just_void(), throwing_executor{});

    bool recv_called{false};
    concore::computation::run_with(c, test_error_receiver{&recv_called});
    CHECK(recv_called);
}

TEST_CASE("on with a cancelled executor that yields the cancellation", "[computation]") {
    concore::static_thread_pool pool{1};
    auto ex = pool.executor();
    pool.stop();

    auto c = on(just_void(), ex);

    bool recv_called{false};
    concore::computation::run_with(c, test_done_receiver{&recv_called});
    pool.wait();
    CHECK(recv_called);
}

TEST_CASE("wait will properly return the value type of a computation", "[computation]") {
    auto c = transform(just_value(16), [](int x) { return x * x; });

    auto res = wait(c);
    CHECK(res == 256);
}

TEST_CASE("wait will properly return the value computed on a different thread", "[computation]") {
    auto c0 = on(just_value(16), concore::spawn_executor{});
    auto c = transform(c0, [](int x) { return x * x; });

    auto res = wait(c);
    CHECK(res == 256);
}

TEST_CASE("wait will ensure that void computation is finished", "[computation]") {
    bool executed{false};
    auto c0 = on(just_void(), concore::spawn_executor{});
    auto c = transform(c0, [&] { executed = true; });

    wait(c);
    CHECK(executed);
}

TEST_CASE("wait will throw error if the computation yields an error", "[computation]") {
    auto c0 = on(just_void(), concore::spawn_executor{});
    auto c = transform(c0, [&] { throw std::logic_error("err"); });

    try {
        wait(c);
        FAIL_CHECK("exception should have been thrown");
    } catch (const std::logic_error&) {
        SUCCEED();
    } catch (...) {
        FAIL_CHECK("invalid type of exception caught");
    }
}

TEST_CASE("wait will throw task_cancelled if the computation is cancelled", "[computation]") {
    concore::static_thread_pool pool{1};
    auto ex = pool.executor();
    pool.stop();

    auto c = on(just_void(), ex);

    try {
        wait(c);
        FAIL_CHECK("exception should have been thrown");
    } catch (const concore::task_cancelled&) {
        SUCCEED();
    } catch (...) {
        FAIL_CHECK("invalid type of exception caught");
    }
}

TEST_CASE("to_task transforms a simple computation into a task", "[computation]") {
    bool executed{false};
    auto c = from_function([&] { executed = true; });
    concore::task t = to_task(c);
    t();
    CHECK(executed);
}

TEST_CASE("to_task transforms a simple computation yielding int into a task", "[computation]") {
    bool executed{false};
    auto c = from_function([&] {
        executed = true;
        return 10;
    });
    concore::task t = to_task(c);
    t();
    CHECK(executed);
}

TEST_CASE("to_task creates task with the given group", "[computation]") {
    auto grp = concore::task_group::create();
    grp.cancel();
    bool executed{false};
    auto c = from_function([&] { executed = true; });
    concore::task t = to_task(c, grp);
    t();
    CHECK_FALSE(executed);
}

TEST_CASE("to_task calls given continuation on success", "[computation]") {
    bool cont_called{false};
    auto cont_fun = [&](std::exception_ptr ex) {
        cont_called = true;
        CHECK_FALSE(ex);
    };
    bool executed{false};
    auto c = from_function([&] { executed = true; });
    concore::task t = to_task(c, {}, cont_fun);
    t();
    CHECK(executed);
    CHECK(cont_called);
}

TEST_CASE("to_task calls given continuation on error", "[computation]") {
    bool cont_called{false};
    auto cont_fun = [&](std::exception_ptr ex) {
        cont_called = true;
        CHECK(ex);
    };
    auto c = from_function([&] { throw std::logic_error("err"); });
    concore::task t = to_task(c, {}, cont_fun);
    t();
    CHECK(cont_called);
}

TEST_CASE("to_task calls given continuation if computation cancelled", "[computation]") {
    auto grp = concore::task_group::create();
    auto t0 = concore::task{[] {}, grp};
    auto c = from_task(std::move(t0));
    grp.cancel();

    bool cont_called{false};
    auto cont_fun = [&](std::exception_ptr ex) {
        cont_called = true;
        CHECK(ex);
        try {
            std::rethrow_exception(ex);
        } catch (const concore::task_cancelled&) {
            SUCCEED("ok");
        } catch (...) {
            FAIL_CHECK("invalid exception");
        }
    };
    concore::task t = to_task(c, {}, cont_fun);
    t();
    CHECK(cont_called);
}

TEST_CASE("run behaves as run_with", "[computation]") {
    int res{0};
    concore::computation::run(just_value(10), test_value_receiver<int>{&res});
    CHECK(res == 10);
}

TEST_CASE("run can take no receiver", "[computation]") {
    bool ftor_called{false};
    auto c = from_function([&] { ftor_called = true; });

    concore::computation::run(c);
    CHECK(ftor_called);
}

TEST_CASE("run_on will run the computation on the given executor (w/ recv)", "[computation]") {
    concore::static_thread_pool pool{1};
    auto ex = pool.executor();

    bool ftor_called{false};
    auto c = from_function([&] {
        ftor_called = true;
        CHECK(ex.running_in_this_thread());
    });

    bool recv_called{false};
    concore::computation::run_on(ex, c, test_void_receiver{&recv_called});
    pool.wait();
    CHECK(recv_called);
    CHECK(ftor_called);
}

TEST_CASE("run_on will run the computation on the given executor (w/o recv)", "[computation]") {
    concore::static_thread_pool pool{1};
    auto ex = pool.executor();

    bool ftor_called{false};
    auto c = from_function([&] {
        ftor_called = true;
        CHECK(ex.running_in_this_thread());
    });

    concore::computation::run_on(ex, c);
    pool.wait();
    CHECK(ftor_called);
}

TEST_CASE("run_on will forward executor exception", "[computation]") {
    bool ftor_called{false};
    auto c = from_function([&] { ftor_called = true; });

    try {
        concore::computation::run_on(throwing_executor{}, c);
        FAIL_CHECK("exception not thrown");
    } catch (...) {
    }
    CHECK_FALSE(ftor_called);
}

TEST_CASE("transform can be piped", "[computation]") {
    auto c = just_value(10) | transform([](int x) { return x * x; });
    int res = wait(c);
    CHECK(res == 100);
}

TEST_CASE("bind can be piped", "[computation]") {
    auto c = just_value(10) | bind([](int x) { return just_value(x * x); });
    int res = wait(c);
    CHECK(res == 100);
}

TEST_CASE("bind_error can be piped", "[computation]") {
    auto f = [] {
        throw std::logic_error("err");
        return 3;
    };
    auto c = from_function(f) | bind_error([](std::exception_ptr) { return just_value(12); });
    int res = wait(c);
    CHECK(res == 12);
}

TEST_CASE("on can be piped", "[computation]") {
    auto c = just_value(10) | on(concore::spawn_executor{});
    int res = wait(c);
    CHECK(res == 10);
}
