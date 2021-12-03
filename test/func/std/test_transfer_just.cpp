#include <catch2/catch.hpp>
#include <concore/sender_algo/transfer_just.hpp>
#include <concore/sender_algo/just_error.hpp>
#include <concore/thread_pool.hpp>
#include <test_common/task_utils.hpp>
#include <test_common/receivers.hpp>
#include <test_common/schedulers.hpp>

TEST_CASE("transfer_just returns a sender", "[sender_algo]") {
    auto snd = concore::transfer_just(inline_scheduler{}, 13);
    static_assert(concore::sender<decltype(snd)>);
    (void)snd;
}
TEST_CASE("transfer_just simple example", "[sender_algo]") {
    inline_scheduler sched;
    auto snd = concore::transfer_just(sched, 13);
    auto op = concore::connect(std::move(snd), expect_value_receiver{13});
    concore::start(op);
    // The receiver checks if we receive the right value
}

TEST_CASE("transfer_just calls the receiver when the scheduler dictates", "[sender_algo]") {
    int recv_value{0};
    impulse_scheduler sched;
    auto snd = concore::transfer_just(sched, 13);
    auto op = concore::connect(snd, expect_value_receiver_ex{&recv_value});
    concore::start(op);
    // Up until this point, the scheduler didn't start any task; no effect expected
    CHECK(recv_value == 0);

    // Tell the scheduler to start executing one task
    sched.start_next();
    CHECK(recv_value == 13);
}

TEST_CASE("transfer_just sender algo calls receiver on the specified scheduler", "[sender_algo]") {
    bool executed = false;
    {
        concore::static_thread_pool pool{1};
        auto sched = pool.scheduler();

        auto s = concore::transfer_just(sched, 1);
        auto recv = make_fun_receiver([&](int val) {
            // Check the content of the value
            REQUIRE(val == 1);
            // Check that this runs in the scheduler thread
            REQUIRE(sched.running_in_this_thread());
            // Mark this as executed
            executed = true;
        });
        static_assert(concore::receiver<decltype(recv)>, "invalid receiver");
        auto op = concore::connect(std::move(s), recv);
        concore::start(op);

        // Wait for the task to be executed
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        REQUIRE(bounded_wait());
    }
    REQUIRE(executed);
}

TEST_CASE("transfer_just can be called with value type scheduler", "[sender_algo]") {
    auto snd = concore::transfer_just(inline_scheduler{}, 13);
    auto op = concore::connect(std::move(snd), expect_value_receiver{13});
    concore::start(op);
    // The receiver checks if we receive the right value
}
TEST_CASE("transfer_just can be called with rvalue ref scheduler", "[sender_algo]") {
    auto snd = concore::transfer_just(inline_scheduler{}, 13);
    auto op = concore::connect(std::move(snd), expect_value_receiver{13});
    concore::start(op);
    // The receiver checks if we receive the right value
}
TEST_CASE("transfer_just can be called with const ref scheduler", "[sender_algo]") {
    const inline_scheduler sched;
    auto snd = concore::transfer_just(sched, 13);
    auto op = concore::connect(std::move(snd), expect_value_receiver{13});
    concore::start(op);
    // The receiver checks if we receive the right value
}
TEST_CASE("transfer_just can be called with ref scheduler", "[sender_algo]") {
    inline_scheduler sched;
    auto snd = concore::transfer_just(sched, 13);
    auto op = concore::connect(std::move(snd), expect_value_receiver{13});
    concore::start(op);
    // The receiver checks if we receive the right value
}

TEST_CASE("transfer_just forwards set_error calls", "[sender_algo]") {
    error_scheduler<std::exception_ptr> sched{std::exception_ptr{}};
    auto snd = concore::transfer_just(sched, 13);
    auto op = concore::connect(std::move(snd), expect_error_receiver{});
    concore::start(op);
    // The receiver checks if we receive an error
}
TEST_CASE("transfer_just forwards set_error calls of other types", "[sender_algo]") {
    error_scheduler<std::string> sched{std::string{"error"}};
    auto snd = concore::transfer_just(sched, 13);
    auto op = concore::connect(std::move(snd), expect_error_receiver{});
    concore::start(op);
    // The receiver checks if we receive an error
}
TEST_CASE("transfer_just forwards set_done calls", "[sender_algo]") {
    done_scheduler sched{};
    auto snd = concore::transfer_just(sched, 13);
    auto op = concore::connect(std::move(snd), expect_done_receiver{});
    concore::start(op);
    // The receiver checks if we receive the done signal
}

template <typename... Ts>
struct type_array {};

TEST_CASE("transfer_just has the values_type corresponding to the given values", "[sender_algo]") {
    inline_scheduler sched{};

    using t1 = decltype(concore::transfer_just(sched, 1));
    using t2 = decltype(concore::transfer_just(sched, 3, 0.14));
    using t3 = decltype(concore::transfer_just(sched, 3, 0.14, std::string{"pi"}));

    using vt1 = concore::sender_traits<t1>::value_types<type_array, type_array>;
    using vt2 = concore::sender_traits<t2>::value_types<type_array, type_array>;
    using vt3 = concore::sender_traits<t3>::value_types<type_array, type_array>;
    static_assert(std::is_same<vt1, type_array<type_array<int>>>::value);
    static_assert(std::is_same<vt2, type_array<type_array<int, double>>>::value);
    static_assert(std::is_same<vt3, type_array<type_array<int, double, std::string>>>::value);
}
// TODO: fix error_types for all the senders involved in the definition of transfer_just
// TEST_CASE("transfer_just keeps error_types from scheduler's sender", "[sender_algo]") {
//     inline_scheduler sched1{};
//     error_scheduler sched2{};
//     error_scheduler<int> sched3{43};
//
//     using t1 = decltype(concore::transfer_just(sched1, 1));
//     using t2 = decltype(concore::transfer_just(sched2, 2));
//     using t3 = decltype(concore::transfer_just(sched3, 3));
//
//     using et1 = concore::sender_traits<t1>::error_types<type_array>;
//     using et2 = concore::sender_traits<t2>::error_types<type_array>;
//     using et3 = concore::sender_traits<t3>::error_types<type_array>;
//
//     static_assert(std::is_same<et1, type_array<std::exception_ptr>>::value);
//     static_assert(std::is_same<et2, type_array<std::exception_ptr>>::value);
//     static_assert(std::is_same<et3, type_array<int, std::exception_ptr>>::value);
// }

TEST_CASE("transfer_just keeps send_done from scheduler's sender", "[sender_algo]") {
    inline_scheduler sched1{};
    error_scheduler sched2{};
    done_scheduler sched3{};

    using t1 = decltype(concore::transfer_just(sched1, 1));
    using t2 = decltype(concore::transfer_just(sched2, 2));
    using t3 = decltype(concore::transfer_just(sched3, 3));

    CHECK_FALSE(concore::sender_traits<t1>::sends_done);
    CHECK_FALSE(concore::sender_traits<t2>::sends_done);
    CHECK(concore::sender_traits<t3>::sends_done);
}

TEST_CASE("transfer_just advertises its completion scheduler", "[sender_algo]") {
    // TODO
}

// Modify the value when we invoke this custom defined transfer_just implementation
auto tag_invoke(decltype(concore::transfer_just), inline_scheduler sched, std::string value) {
    return concore::on(concore::just("Hello, " + value), sched);
}

TEST_CASE("transfer_just can be customized", "[sender_algo]") {
    // The customization will alter the value passed in
    auto snd = concore::transfer_just(inline_scheduler{}, std::string{"world"});
    std::string res;
    auto op = concore::connect(std::move(snd), expect_value_receiver_ex<std::string>(&res));
    concore::start(op);
    REQUIRE(res == "Hello, world");
}
