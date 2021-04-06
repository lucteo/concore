#include <catch2/catch.hpp>
#include <concore/sender_algo/just.hpp>
#include <concore/sender_algo/on.hpp>
#include <concore/execution.hpp>
#include <concore/thread_pool.hpp>
#include <test_common/task_utils.hpp>

struct expect_receiver_base {
    void set_done() noexcept { FAIL("Done called"); }
    void set_error(std::exception_ptr eptr) noexcept {
        try {
            if (eptr)
                std::rethrow_exception(eptr);
            FAIL("Empty exception thrown");
        } catch (const std::exception& e) {
            FAIL("Exception thrown: " << e.what());
        }
    }
};

template <typename T>
struct expect_receiver : expect_receiver_base {
    T val_;

    expect_receiver(T val)
        : val_(std::move(val)) {}

    //! Called whenever the sender completed the work with success
    void set_value(const T& val) { REQUIRE(val == val_); }
};

template <typename T>
struct fun_receiver : expect_receiver_base {
    using fun_t = std::function<void(T)>;
    fun_t f_;

    template <typename F>
    fun_receiver(F f)
        : f_(std::forward<F>(f)) {}

    //! Called whenever the sender completed the work with success
    void set_value(const T& val) { f_(std::move(val)); }
};

template <typename T>
expect_receiver<T> make_expect_receiver(T val) {
    return expect_receiver<T>{std::move(val)};
}

template <typename T, typename F>
fun_receiver<T> make_fun_receiver(F f) {
    return fun_receiver<T>{std::forward<F>(f)};
}

TEST_CASE("Simple test for just", "[sender_algo]") {
    concore::just(1).connect(make_expect_receiver(1)).start();
    concore::just(2).connect(make_expect_receiver(2)).start();
    concore::just(3).connect(make_expect_receiver(3)).start();

    concore::just(std::string("this")).connect(make_expect_receiver(std::string("this"))).start();
    concore::just(std::string("that")).connect(make_expect_receiver(std::string("that"))).start();
}

TEST_CASE("just returns a sender", "[sender_algo]") {
    using t = decltype(concore::just(1));
    static_assert(concore::sender<t>, "concore::just must return a sender");
    REQUIRE(concore::sender<t>);
}

TEST_CASE("on sender algo calls receiver on the specified scheduler", "[sender_algo]") {
    bool executed = false;
    {
        concore::static_thread_pool pool{1};
        auto sched = pool.scheduler();

        auto s1 = concore::just(1);
        auto s2 = concore::on(s1, sched);
        auto recv = make_fun_receiver<int>([&](int val) {
            // Check the content of the value
            REQUIRE(val == 1);
            // Check that this runs in the scheduler thread
            REQUIRE(sched.running_in_this_thread());
            // Mark this as executed
            executed = true;
        });
        static_assert(concore::receiver<decltype(recv)>, "invalid receiver");
        // concore::submit(s2, recv);
        auto op = concore::connect(s2, recv);
        concore::start(op);
        // TODO: check why submit doesn't work

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
