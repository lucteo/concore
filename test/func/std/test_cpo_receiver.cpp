#include <catch2/catch.hpp>
#include <concore/execution.hpp>
#include <concore/thread_pool.hpp>

using namespace concore::detail::cpo_set_error;

template <typename R> // requires meets_outer_fun<R, std::bad_alloc>
void test_receiver_done_and_error(R&& r) {
    concore::set_done(r);
    CHECK(r.state_ == 1);
    concore::set_error(r, std::bad_alloc{});
    CHECK(r.state_ == 2);
}

template <typename R>
void test_receiver0(R&& r) {
    test_receiver_done_and_error(r);
    concore::set_value(r);
    CHECK(r.state_ == 0);
}

template <typename R>
void test_receiver1(R&& r) {
    test_receiver_done_and_error(r);
    concore::set_value(r, 10);
    CHECK(r.state_ == 0);
    CHECK(r.val1_ == 10);
}

template <typename R>
void test_receiver2(R&& r) {
    test_receiver_done_and_error(r);
    concore::set_value(r, 10, 3.14);
    CHECK(r.state_ == 0);
    CHECK(r.val1_ == 10);
    CHECK(r.val2_ == 3.14);
}

namespace NS1 {
struct my_receiver {
    friend inline bool operator==(my_receiver, my_receiver) { return false; }
    friend inline bool operator!=(my_receiver, my_receiver) { return true; }

    void set_value() { state_ = 0; }

    void set_value(int x) {
        state_ = 0;
        val1_ = x;
    }

    void set_value(int x, double y) {
        state_ = 0;
        val1_ = x;
        val2_ = y;
    }

    void set_done() noexcept { state_ = 1; }

    template <typename E>
    void set_error(E) noexcept {
        state_ = 2;
    }

    int state_{-1};
    int val1_{0};
    double val2_{0.0};
};
} // namespace NS1

TEST_CASE("receiver object with inner methods", "[execution][cpo_receiver]") {
    test_receiver0(NS1::my_receiver{});
    test_receiver1(NS1::my_receiver{});
    test_receiver2(NS1::my_receiver{});
}

namespace NS2 {
struct my_receiver {
    friend inline bool operator==(my_receiver, my_receiver) { return false; }
    friend inline bool operator!=(my_receiver, my_receiver) { return true; }

    int state_{-1};
    int val1_{0};
    double val2_{0.0};
};

void set_value(my_receiver& r) { r.state_ = 0; }

void set_value(my_receiver& r, int x) {
    r.state_ = 0;
    r.val1_ = x;
}

void set_value(my_receiver& r, int x, double y) {
    r.state_ = 0;
    r.val1_ = x;
    r.val2_ = y;
}

void set_done(my_receiver& r) noexcept { r.state_ = 1; }

template <typename E>
void set_error(my_receiver& r, E) noexcept {
    r.state_ = 2;
}

} // namespace NS2

TEST_CASE("receiver object with other functions", "[execution][cpo_receiver]") {
    test_receiver0(NS2::my_receiver{});
    test_receiver1(NS2::my_receiver{});
    test_receiver2(NS2::my_receiver{});
}

namespace NS3 {
struct my_receiver {
    friend inline bool operator==(my_receiver, my_receiver) { return false; }
    friend inline bool operator!=(my_receiver, my_receiver) { return true; }

    int state_{-1};
    int val1_{0};
    double val2_{0.0};
};

using concore::set_done_t;
using concore::set_error_t;
using concore::set_value_t;

void tag_invoke(set_value_t, my_receiver& r) { r.state_ = 0; }

void tag_invoke(set_value_t, my_receiver& r, int x) {
    r.state_ = 0;
    r.val1_ = x;
}

void tag_invoke(set_value_t, my_receiver& r, int x, double y) {
    r.state_ = 0;
    r.val1_ = x;
    r.val2_ = y;
}

void tag_invoke(set_done_t, my_receiver& r) noexcept { r.state_ = 1; }

template <typename E>
void tag_invoke(set_error_t, my_receiver& r, E) noexcept {
    r.state_ = 2;
}

} // namespace NS3

TEST_CASE("receiver object with tag_invoke specialization", "[execution][cpo_receiver]") {
    test_receiver0(NS3::my_receiver{});
    test_receiver1(NS3::my_receiver{});
    test_receiver2(NS3::my_receiver{});
}
