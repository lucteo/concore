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
#include <test_common/task_utils.hpp>
#include <test_common/receivers.hpp>

TEST_CASE("on sender algo calls receiver on the specified scheduler", "[sender_algo]") {
    bool executed = false;
    {
        concore::static_thread_pool pool{1};
        auto sched = pool.scheduler();

        auto s1 = concore::just(1);
        auto s2 = concore::on(s1, sched);
        auto recv = make_fun_receiver([&](int val) {
            // Check the content of the value
            REQUIRE(val == 1);
            // Check that this runs in the scheduler thread
            REQUIRE(sched.running_in_this_thread());
            // Mark this as executed
            executed = true;
        });
        static_assert(concore::receiver<decltype(recv)>, "invalid receiver");
        auto op = concore::connect(std::move(s2), recv);
        concore::start(op);

        // Wait for the task to be executed
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        REQUIRE(bounded_wait());
    }
    REQUIRE(executed);
}

TEST_CASE("on returns a sender", "[sender_algo]") {
    using t = decltype(concore::on(concore::just(1), concore::static_thread_pool{1}.scheduler()));
    static_assert(concore::sender<t>, "concore::on must return a sender");
    REQUIRE(concore::sender<t>);
}

TEST_CASE("on forwards the return type", "[sender_algo]") {
    using st_int =
            decltype(concore::on(concore::just(1), concore::static_thread_pool{1}.scheduler()));
    using st_double =
            decltype(concore::on(concore::just(3.14), concore::static_thread_pool{1}.scheduler()));

    using concore::detail::sender_single_return_type;
    static_assert(std::is_same_v<sender_single_return_type<st_int>, int>,
            "Improper return type for `on`");
    static_assert(std::is_same_v<sender_single_return_type<st_double>, double>,
            "Improper return type for `on`");
}

TEST_CASE("just_on sender algo calls receiver on the specified scheduler", "[sender_algo]") {
    bool executed = false;
    {
        concore::static_thread_pool pool{1};
        auto sched = pool.scheduler();

        auto s = concore::just_on(sched, 1);
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

TEST_CASE("just_on returns a sender", "[sender_algo]") {
    using t = decltype(concore::just_on(concore::static_thread_pool{1}.scheduler(), 1));
    static_assert(concore::sender<t>, "concore::just_on must return a sender");
    REQUIRE(concore::sender<t>);
}

TEST_CASE("sync_wait on simple just senders", "[sender_algo]") {
    auto r1 = concore::sync_wait(concore::just(1));
    auto r2 = concore::sync_wait(concore::just(2));
    auto r3 = concore::sync_wait(concore::just(3));
    auto rs1 = concore::sync_wait(concore::just(std::string{"this"}));

    REQUIRE(r1 == 1);
    REQUIRE(r2 == 2);
    REQUIRE(r3 == 3);
    REQUIRE(rs1 == "this");
}

TEST_CASE("sync_wait on simple just_on senders", "[sender_algo]") {
    concore::static_thread_pool pool{1};
    auto sched = pool.scheduler();

    auto r1 = concore::sync_wait(concore::just_on(sched, 1));
    REQUIRE(r1 == 1);

    auto r2 = concore::sync_wait(concore::just_on(sched, 3.14));
    REQUIRE(r2 == 3.14);
}

TEST_CASE("sync_wait_r works with conversions", "[sender_algo]") {
    auto r1 = concore::sync_wait_r<double>(concore::just(1));
    auto r2 = concore::sync_wait_r<double>(concore::just(3.14));

    REQUIRE(r1 == 1.0);
    REQUIRE(r2 == 3.14);
}

TEST_CASE("transform on simple just senders", "[sender_algo]") {
    auto f = [](int x) { return x * x; };
    auto r1 = concore::sync_wait(concore::transform(concore::just(1), f));
    auto r2 = concore::sync_wait(concore::transform(concore::just(2), f));
    auto r3 = concore::sync_wait(concore::transform(concore::just(3), f));

    REQUIRE(r1 == 1);
    REQUIRE(r2 == 4);
    REQUIRE(r3 == 9);
}

TEST_CASE("transform returns a sender", "[sender_algo]") {
    auto f = [](int x) { return x * x; };
    REQUIRE(f(2) == 4);
    using t = decltype(concore::transform(concore::just(1), f));
    static_assert(concore::sender<t>, "concore::transform must return a sender");
    REQUIRE(concore::sender<t>);
}

TEST_CASE("transform can change the return type", "[sender_algo]") {
    auto f = [](int x) { return double(x * x); };
    auto r = concore::sync_wait(concore::transform(concore::just(2), f));
    using t = decltype(r);
    static_assert(std::is_same_v<t, double>, "transformed sender must return a double");
    REQUIRE(r == 4.0);
}

TEST_CASE("transform can handle multiple values", "[sender_algo]") {
    auto f = [](int x, double d) { return x + d; };
    auto r = concore::sync_wait(concore::transform(concore::just(3, 0.14), f));
    REQUIRE(r == 3.14);
}

TEST_CASE("let_value with simple just senders", "[sender_algo]") {
    auto let_value_fun = [](int& let_v) {
        return concore::transform(concore::just(4), [&](int v) { return let_v + v; });
    };
    auto s = concore::let_value(concore::just(3), std::move(let_value_fun));
    REQUIRE(concore::sync_wait(s) == 7);
}

TEST_CASE("let_value returns a sender", "[sender_algo]") {
    auto let_value_fun = [](int& let_v) {
        return concore::transform(concore::just(4), [&](int v) { return let_v + v; });
    };
    CHECK(let_value_fun);
    using t = decltype(concore::let_value(concore::just(3), std::move(let_value_fun)));
    static_assert(concore::sender<t>, "concore::let_value must return a sender");
    REQUIRE(concore::sender<t>);
}

TEST_CASE("let_value invoked on a different thread", "[sender_algo]") {
    auto let_value_fun = [](int& let_v) {
        return concore::transform(concore::just(4), [&](int v) { return let_v + v; });
    };

    concore::static_thread_pool pool{1};
    auto sched = pool.scheduler();

    auto s = concore::let_value(concore::just_on(sched, 3), std::move(let_value_fun));
    REQUIRE(concore::sync_wait(s) == 7);
}

TEST_CASE("let_value invokes scheduler on a different thread", "[sender_algo]") {
    concore::static_thread_pool pool{1};
    auto sched = pool.scheduler();

    auto let_value_fun = [&](int& let_v) {
        // we can't use reference on the next line anymore -- dangling reference
        return concore::transform(concore::just_on(sched, 4), [=](int v) { return let_v + v; });
    };

    auto s = concore::let_value(concore::just(3), std::move(let_value_fun));
    REQUIRE(concore::sync_wait(s) == 7);
}

TEST_CASE("when_all with two simple just senders", "[sender_algo]") {
    auto s = concore::when_all(concore::just(3), concore::just(0.14));
    auto ssum = concore::transform(std::move(s), [](int x, double y) { return x + y; });
    REQUIRE(concore::sync_wait(ssum) == 3.14);
}

TEST_CASE("when_all returns a sender", "[sender_algo]") {
    using t = decltype(concore::when_all(concore::just(3), concore::just(0.14)));
    static_assert(concore::sender<t>, "concore::when_all must return a sender");
    REQUIRE(concore::sender<t>);
}

TEST_CASE("when_all works with a single sender", "[sender_algo]") {
    auto s = concore::when_all(concore::just(3));
    REQUIRE(concore::sync_wait(s) == 3);
}

TEST_CASE("piping works for 'on'", "[sender_algo]") {
    concore::static_thread_pool pool{1};
    auto sched = pool.scheduler();

    auto s = concore::just(3) | concore::on(sched);
    REQUIRE(concore::sync_wait(s) == 3);

    auto s2 = concore::just(3) | concore::on(sched) | concore::on(sched);
    REQUIRE(concore::sync_wait(s2) == 3);
}

TEST_CASE("sender can be passed at a later point for 'on'", "[sender_algo]") {
    concore::static_thread_pool pool{1};
    auto sched = pool.scheduler();

    auto s = concore::on(sched)(concore::just(3));
    REQUIRE(concore::sync_wait(s) == 3);

    auto s2 = concore::on(sched)(concore::just(3)) | concore::on(sched);
    REQUIRE(concore::sync_wait(s2) == 3);
}

TEST_CASE("piping works for 'transform'", "[sender_algo]") {
    auto f = [](int x) { return x * x; };
    auto s = concore::just(3) | concore::transform(f);
    REQUIRE(concore::sync_wait(s) == 9);
}

TEST_CASE("sender can be passed at a later point for 'transform'", "[sender_algo]") {
    auto f = [](int x) { return x * x; };
    auto s = concore::transform(f)(concore::just(3));
    REQUIRE(concore::sync_wait(s) == 9);
}

TEST_CASE("piping works for 'sync_wait'", "[sender_algo]") {
    auto f = [](int x) { return x * x; };
    auto val = concore::just(3) | concore::transform(f) | concore::sync_wait();
    REQUIRE(val == 9);
}

TEST_CASE("sender can be passed at a later point for 'sync_wait'", "[sender_algo]") {
    auto val = concore::sync_wait()(concore::just(3));
    REQUIRE(val == 3);
}

TEST_CASE("piping works for 'sync_wait_r'", "[sender_algo]") {
    auto f = [](int x) { return x * x; };
    auto val = concore::just(3) | concore::transform(f) | concore::sync_wait_r<double>();
    REQUIRE(val == 9.0);
}

TEST_CASE("sender can be passed at a later point for 'sync_wait_r'", "[sender_algo]") {
    auto val = concore::sync_wait_r<double>()(concore::just(3));
    REQUIRE(val == 3.0);
}

TEST_CASE("piping works for 'let_value'", "[sender_algo]") {
    auto let_value_fun = [](int& let_v) {
        return concore::transform(concore::just(4), [&](int v) { return let_v + v; });
    };
    auto s = concore::just(3) | concore::let_value(let_value_fun);
    REQUIRE(concore::sync_wait(s) == 7);
}

TEST_CASE("sender can be passed at a later point for 'let_value'", "[sender_algo]") {
    auto let_value_fun = [](int& let_v) {
        return concore::transform(concore::just(4), [&](int v) { return let_v + v; });
    };
    auto s = concore::let_value(let_value_fun)(concore::just(3));
    REQUIRE(concore::sync_wait(s) == 7);
}

TEST_CASE("partial piping of sender algorithms", "[sender_algo]") {
    auto f1 = [](int x) { return x * x; };
    auto f2 = [](int x) { return 2 * x; };
    auto s = concore::just(3) | (concore::transform(f1) | concore::transform(f2));
    REQUIRE(concore::sync_wait(s) == 18);
}

TEST_CASE("partial piping of sender algorithms with sender at the end", "[sender_algo]") {
    auto f1 = [](int x) { return x * x; };
    auto f2 = [](int x) { return 2 * x; };
    auto s = (concore::transform(f1) | concore::transform(f2))(concore::just(3));
    REQUIRE(concore::sync_wait(s) == 18);
}
