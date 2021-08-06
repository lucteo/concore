#include <catch2/catch.hpp>
#include <concore/sender_algo/just.hpp>
#include <concore/sender_algo/on.hpp>
#include <concore/sender_algo/just_on.hpp>
#include <concore/sender_algo/sync_wait.hpp>
#include <concore/sender_algo/transform.hpp>
#include <concore/sender_algo/let_value.hpp>
#include <concore/sender_algo/when_all.hpp>
#include <concore/as_operation.hpp>
#include <concore/as_sender.hpp>
#include <concore/detail/sender_helpers.hpp>
#include <concore/thread_pool.hpp>
#include <test_common/throwing_executor.hpp>

struct expect_done_receiver {
    bool* executed_;

    template <typename... Ts>
    void set_value(Ts...) noexcept {
        REQUIRE(false);
    }
    void set_done() noexcept { *executed_ = true; }
    void set_error(std::exception_ptr) noexcept { REQUIRE(false); }
};

struct expect_error_receiver {
    bool* executed_;

    template <typename... Ts>
    void set_value(Ts...) noexcept {
        REQUIRE(false);
    }
    void set_done() noexcept { REQUIRE(false); }
    void set_error(std::exception_ptr) noexcept { *executed_ = true; }
};

auto void_throwing_sender() { return concore::as_sender<throwing_executor>{{}}; }

auto int_throwing_sender() {
    concore::as_sender<throwing_executor> sender1{{}};
    return concore::transform(sender1, []() { return 3; });
}

struct throwing_scheduler {
    concore::as_sender<throwing_executor> schedule() noexcept {
        return concore::as_sender<throwing_executor>{{}};
    }

    friend inline bool operator==(throwing_scheduler, throwing_scheduler) { return true; }
    friend inline bool operator!=(throwing_scheduler, throwing_scheduler) { return false; }
};

TEST_CASE("Thread pool's sender calls set_done when the pool was stopped", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    auto scheduler = my_pool.scheduler();

    bool executed = false;
    auto op = scheduler.schedule().connect(expect_done_receiver{&executed});

    // Stop the pool, so that the new operations are cancelled
    my_pool.stop();

    // Now start the task, and expect 'set_done()' to be called
    concore::start(op);

    // Ensure that the receiver is called
    my_pool.wait();
    REQUIRE(executed);
}

TEST_CASE("as_operation calls set_error when executor throws", "[sender_algo]") {
    bool executed{false};
    concore::as_operation<throwing_executor, expect_error_receiver> op{{}, {&executed}};
    op.start();
    REQUIRE(executed);
}

TEST_CASE("as_operation calls set_done when executor cancelled execution", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    auto ex = my_pool.executor();
    my_pool.stop();

    bool executed{false};
    concore::as_operation<decltype(ex), expect_done_receiver> op{ex, {&executed}};
    op.start();
    my_pool.wait();
    REQUIRE(executed);
}

TEST_CASE("as_sender calls set_error when executor throws", "[sender_algo]") {
    bool executed{false};
    concore::as_sender<throwing_executor> sender{{}};
    concore::submit(std::move(sender), expect_error_receiver{&executed});
    REQUIRE(executed);
}

TEST_CASE("as_sender calls set_done when executor cancelled execution", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    auto ex = my_pool.executor();
    my_pool.stop();

    bool executed{false};
    concore::as_sender<decltype((ex))> sender{ex};
    concore::submit(std::move(sender), expect_done_receiver{&executed});
    my_pool.wait();
    REQUIRE(executed);
}

TEST_CASE("on calls set_error when base sender reports error", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};

    bool executed{false};
    auto sender1 = concore::on(my_pool.scheduler().schedule(), throwing_scheduler{});
    concore::submit(std::move(sender1), expect_error_receiver{&executed});
    my_pool.wait();
    REQUIRE(executed);
}

TEST_CASE("on calls set_error when scheduler reports error", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};

    bool executed{false};
    concore::as_sender<throwing_executor> sender{{}};
    auto sender1 = on(sender, my_pool.scheduler());
    concore::submit(std::move(sender1), expect_error_receiver{&executed});
    my_pool.wait();
    REQUIRE(executed);
}

TEST_CASE("on calls set_done when base sender cancelled execution", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    concore::static_thread_pool my_pool2{1};
    my_pool.stop();

    bool executed{false};
    auto sender1 = concore::on(my_pool.scheduler().schedule(), my_pool2.scheduler());
    concore::submit(std::move(sender1), expect_done_receiver{&executed});
    my_pool.wait();
    my_pool2.wait();
    REQUIRE(executed);
}

TEST_CASE("on calls set_done when scheduler cancelled execution", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    concore::static_thread_pool my_pool2{1};
    my_pool2.stop();

    bool executed{false};
    auto sender1 = concore::on(my_pool.scheduler().schedule(), my_pool2.scheduler());
    concore::submit(std::move(sender1), expect_done_receiver{&executed});
    my_pool.wait();
    my_pool2.wait();
    REQUIRE(executed);
}

TEST_CASE("sync_wait raises exception if set_error is called", "[sender_algo]") {
    concore::as_sender<throwing_executor> sender_void{{}};
    bool exception_caught{false};
    try {
        int res = concore::sync_wait(int_throwing_sender());
        (void)res;
        REQUIRE(false);
    } catch (...) {
        exception_caught = true;
    }
    REQUIRE(exception_caught);
}

TEST_CASE("transform propagates set_error", "[sender_algo]") {
    auto sender = concore::transform(int_throwing_sender(), [](int x) { return x * x; });

    bool executed{false};
    concore::submit(std::move(sender), expect_error_receiver{&executed});
    REQUIRE(executed);
}

TEST_CASE("transform propagates set_done", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    auto scheduler = my_pool.scheduler();
    my_pool.stop();

    bool fun_executed{false};
    auto sender = concore::transform(scheduler.schedule(), [&]() { fun_executed = true; });

    bool executed{false};
    concore::submit(std::move(sender), expect_done_receiver{&executed});
    my_pool.wait();
    REQUIRE(executed);
    REQUIRE_FALSE(fun_executed);
}

TEST_CASE("transform calls set_error if the function throws", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    auto scheduler = my_pool.scheduler();

    auto f = []() { throw std::logic_error("error"); };
    auto sender = concore::transform(scheduler.schedule(), std::move(f));

    bool executed{false};
    concore::submit(std::move(sender), expect_error_receiver{&executed});
    my_pool.wait();
    REQUIRE(executed);
}

TEST_CASE("let_value propagates set_error", "[sender_algo]") {
    auto let_value_fun = [](int& let_v) {
        return concore::transform(concore::just(4), [&](int v) { return let_v + v; });
    };
    auto s = concore::let_value(int_throwing_sender(), std::move(let_value_fun));

    bool executed{false};
    concore::submit(std::move(s), expect_error_receiver{&executed});
    REQUIRE(executed);
}

TEST_CASE("let_value propagates set_done", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    my_pool.stop();

    bool fun_executed{false};
    auto let_value_fun = [&](int& let_v) {
        fun_executed = true;
        return concore::transform(concore::just(4), [](int) {});
    };
    auto sender0 = concore::just_on(my_pool.scheduler(), 3);
    auto s = concore::let_value(std::move(sender0), std::move(let_value_fun));

    bool executed{false};
    concore::submit(std::move(s), expect_done_receiver{&executed});
    my_pool.wait();
    REQUIRE(executed);
    REQUIRE_FALSE(fun_executed);
}

TEST_CASE("let_value calls set_error if the function throws", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};

    auto let_value_fun = [&](int& let_v) {
        throw std::logic_error("error");
        return concore::transform(concore::just(4), [](int) {});
    };
    auto sender0 = concore::just_on(my_pool.scheduler(), 3);
    auto s = concore::let_value(std::move(sender0), std::move(let_value_fun));

    bool executed{false};
    concore::submit(std::move(s), expect_error_receiver{&executed});
    my_pool.wait();
    REQUIRE(executed);
}

TEST_CASE("when_all propagates set_error", "[sender_algo]") {
    auto s = concore::when_all(concore::just(2), int_throwing_sender());

    bool executed{false};
    concore::submit(std::move(s), expect_error_receiver{&executed});
    REQUIRE(executed);
}

TEST_CASE("when_all propagates set_done", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    auto scheduler = my_pool.scheduler();
    my_pool.stop();

    auto s = concore::when_all(concore::just(2), concore::just_on(scheduler, 3));

    bool executed{false};
    concore::submit(std::move(s), expect_done_receiver{&executed});
    my_pool.wait();
    REQUIRE(executed);
}
