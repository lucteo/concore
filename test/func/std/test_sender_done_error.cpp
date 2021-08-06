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
#include <concore/execution.hpp>
#include <concore/thread_pool.hpp>
#include <test_common/task_utils.hpp>
#include <test_common/throwing_executor.hpp>

struct expect_done_receiver {
    bool* executed_;

    void set_value() noexcept { REQUIRE(false); }
    void set_done() noexcept { *executed_ = true; }
    void set_error(std::exception_ptr) noexcept { REQUIRE(false); }
};

struct expect_error_receiver {
    bool* executed_;

    void set_value() noexcept { REQUIRE(false); }
    void set_done() noexcept { REQUIRE(false); }
    void set_error(std::exception_ptr) noexcept { *executed_ = true; }
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
    auto op = concore::connect(sender, expect_error_receiver{&executed});
    op.start();
    REQUIRE(executed);
}

TEST_CASE("as_sender calls set_done when executor cancelled execution", "[sender_algo]") {
    concore::static_thread_pool my_pool{1};
    auto ex = my_pool.executor();
    my_pool.stop();

    bool executed{false};
    concore::as_sender<decltype((ex))> sender{ex};
    auto op = concore::connect(sender, expect_done_receiver{&executed});
    op.start();
    my_pool.wait();
    REQUIRE(executed);
}
