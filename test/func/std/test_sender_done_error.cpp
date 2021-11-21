#include <catch2/catch.hpp>
#include <concore/sender_algo/just.hpp>
#include <concore/sender_algo/on.hpp>
#include <concore/sender_algo/just_on.hpp>
#include <concore/sender_algo/sync_wait.hpp>
#include <concore/sender_algo/transform.hpp>
#include <concore/sender_algo/let_value.hpp>
#include <concore/sender_algo/when_all.hpp>
#include <concore/detail/sender_helpers.hpp>
#include <concore/thread_pool.hpp>
#include <test_common/receivers.hpp>

auto int_throwing_sender() {
    return concore::transform(concore::just(3), [](int x) {
        throw std::logic_error("err");
        return x;
    });
}

auto void_throwing_sender() {
    return concore::transform(concore::just(3), [](int x) { throw std::logic_error("err"); });
}

struct throwing_scheduler {
    friend auto tag_invoke(concore::schedule_t, throwing_scheduler) noexcept {
        return void_throwing_sender();
    }

    friend inline bool operator==(throwing_scheduler, throwing_scheduler) { return true; }
    friend inline bool operator!=(throwing_scheduler, throwing_scheduler) { return false; }
};

TEST_CASE("Thread pool's sender calls set_done when the pool was stopped", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    auto scheduler = my_pool.scheduler();

    bool executed = false;
    auto op = concore::schedule(scheduler).connect(expect_done_receiver_ex{&executed});

    // Stop the pool, so that the new operations are cancelled
    my_pool.stop();

    // Now start the task, and expect 'set_done()' to be called
    concore::start(op);

    // Ensure that the receiver is called
    my_pool.wait();
    REQUIRE(executed);
}

TEST_CASE("on calls set_error when base sender reports error", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};

    bool executed{false};
    auto sender1 = concore::on(concore::schedule(my_pool.scheduler()), throwing_scheduler{});
    auto op = concore::connect(std::move(sender1), expect_error_receiver_ex{&executed});
    concore::start(op);
    my_pool.wait();
    REQUIRE(executed);
}

TEST_CASE("on calls set_error when scheduler reports error", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};

    bool executed{false};
    auto sender1 = concore::on(void_throwing_sender(), my_pool.scheduler());
    auto op = concore::connect(std::move(sender1), expect_error_receiver_ex{&executed});
    concore::start(op);
    my_pool.wait();
    REQUIRE(executed);
}

TEST_CASE("on calls set_done when base sender cancelled execution", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    concore::static_thread_pool my_pool2{1};
    my_pool.stop();

    bool executed{false};
    auto sender1 = concore::on(concore::schedule(my_pool.scheduler()), my_pool2.scheduler());
    auto op = concore::connect(std::move(sender1), expect_done_receiver_ex{&executed});
    concore::start(op);
    my_pool.wait();
    my_pool2.wait();
    REQUIRE(executed);
}

TEST_CASE("on calls set_done when scheduler cancelled execution", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    concore::static_thread_pool my_pool2{1};
    my_pool2.stop();

    bool executed{false};
    auto sender1 = concore::on(concore::schedule(my_pool.scheduler()), my_pool2.scheduler());
    auto op = concore::connect(std::move(sender1), expect_done_receiver_ex{&executed});
    concore::start(op);
    my_pool.wait();
    my_pool2.wait();
    REQUIRE(executed);
}

TEST_CASE("sync_wait raises exception if set_error is called", "[sender_algo]") {
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
    auto op = concore::connect(sender, expect_error_receiver_ex{&executed});
    concore::start(op);
    REQUIRE(executed);
}

TEST_CASE("transform propagates set_done", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    auto scheduler = my_pool.scheduler();
    my_pool.stop();

    bool fun_executed{false};
    auto sender = concore::transform(concore::schedule(scheduler), [&]() { fun_executed = true; });

    bool executed{false};
    auto op = concore::connect(std::move(sender), expect_done_receiver_ex{&executed});
    concore::start(op);
    my_pool.wait();
    REQUIRE(executed);
    REQUIRE_FALSE(fun_executed);
}

TEST_CASE("transform calls set_error if the function throws", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    auto scheduler = my_pool.scheduler();

    auto f = []() { throw std::logic_error("error"); };
    auto sender = concore::transform(concore::schedule(scheduler), std::move(f));

    bool executed{false};
    auto op = concore::connect(std::move(sender), expect_error_receiver_ex{&executed});
    concore::start(op);
    my_pool.wait();
    REQUIRE(executed);
}

TEST_CASE("let_value propagates set_error", "[sender_algo]") {
    auto let_value_fun = [](int& let_v) {
        return concore::transform(concore::just(4), [&](int v) { return let_v + v; });
    };
    auto s = concore::let_value(int_throwing_sender(), std::move(let_value_fun));

    bool executed{false};
    auto op = concore::connect(s, expect_error_receiver_ex{&executed});
    concore::start(op);
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
    auto op = concore::connect(std::move(s), expect_done_receiver_ex{&executed});
    concore::start(op);
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
    auto op = concore::connect(std::move(s), expect_error_receiver_ex{&executed});
    concore::start(op);
    my_pool.wait();
    REQUIRE(executed);
}

TEST_CASE("when_all propagates set_error", "[sender_algo]") {
    auto s = concore::when_all(concore::just(2), int_throwing_sender());

    bool executed{false};
    auto op = concore::connect(std::move(s), expect_error_receiver_ex{&executed});
    concore::start(op);
    REQUIRE(executed);
}

TEST_CASE("when_all propagates set_done", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    auto scheduler = my_pool.scheduler();
    my_pool.stop();

    auto s = concore::when_all(concore::just(2), concore::just_on(scheduler, 3));

    bool executed{false};
    auto op = concore::connect(std::move(s), expect_done_receiver_ex{&executed});
    concore::start(op);
    my_pool.wait();
    REQUIRE(executed);
}
