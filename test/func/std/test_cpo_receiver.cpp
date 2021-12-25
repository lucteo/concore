#include <catch2/catch.hpp>
#include <concore/execution_old.hpp>
#include <test_common/receivers.hpp>

#include <string>
#include <climits>

using concore::set_done_t;
using concore::set_error_t;
using concore::set_value_t;
using concore::detail::cpo_set_done::has_set_done;
using concore::detail::cpo_set_error::has_set_error;
using concore::detail::cpo_set_value::has_set_value;

struct recv_value {
    int* target_;

    friend void tag_invoke(set_value_t, recv_value self, int val) { *self.target_ = val; }
    friend void tag_invoke(set_error_t, recv_value self, int ec) { *self.target_ = -ec; }
    friend void tag_invoke(set_done_t, recv_value self) { *self.target_ = INT_MAX; }
};
struct recv_rvalref {
    int* target_;

    friend void tag_invoke(set_value_t, recv_rvalref&& self, int val) { *self.target_ = val; }
    friend void tag_invoke(set_error_t, recv_rvalref&& self, int ec) { *self.target_ = -ec; }
    friend void tag_invoke(set_done_t, recv_rvalref&& self) { *self.target_ = INT_MAX; }
};
struct recv_ref {
    int* target_;

    friend void tag_invoke(set_value_t, recv_ref& self, int val) { *self.target_ = val; }
    friend void tag_invoke(set_error_t, recv_ref& self, int ec) { *self.target_ = -ec; }
    friend void tag_invoke(set_done_t, recv_ref& self) { *self.target_ = INT_MAX; }
};
struct recv_cref {
    int* target_;

    friend void tag_invoke(set_value_t, const recv_cref& self, int val) { *self.target_ = val; }
    friend void tag_invoke(set_error_t, const recv_cref& self, int ec) { *self.target_ = -ec; }
    friend void tag_invoke(set_done_t, const recv_cref& self) { *self.target_ = INT_MAX; }
};

TEST_CASE("can call set_value on a void receiver", "[execution][cpo_receiver]") {
    concore::set_value(expect_void_receiver{});
}

TEST_CASE("can call set_value on a int receiver", "[execution][cpo_receiver]") {
    concore::set_value(expect_value_receiver{10}, 10);
}

TEST_CASE("can call set_value on a string receiver", "[execution][cpo_receiver]") {
    concore::set_value(expect_value_receiver<std::string>{"hello"}, std::string{"hello"});
}

TEST_CASE("can call set_done on a receiver", "[execution][cpo_receiver]") {
    concore::set_done(expect_done_receiver{});
}

TEST_CASE("can call set_error on a receiver", "[execution][cpo_receiver]") {
    std::exception_ptr ex;
    try {
        throw std::bad_alloc{};
    } catch (...) {
        ex = std::current_exception();
    }
    concore::set_error(expect_error_receiver{}, ex);
}

TEST_CASE("can call set_error with an error code on a receiver", "[execution][cpo_receiver]") {
    std::error_code errCode{100, std::generic_category()};
    concore::set_error(expect_error_receiver{}, errCode);
}

TEST_CASE("set_value with a value passes the value to the receiver", "[execution][cpo_receiver]") {
    concore::set_value(expect_value_receiver<int>{10}, 10);
}

TEST_CASE("can call set_value on a receiver with plain value type", "[execution][cpo_receiver]") {
    static_assert(has_set_value<recv_value, int>, "cannot call set_value on recv_value");
    int val = 0;
    concore::set_value(recv_value{&val}, 10);
    REQUIRE(val == 10);
}
TEST_CASE("can call set_value on a receiver with r-value ref type", "[execution][cpo_receiver]") {
    static_assert(has_set_value<recv_rvalref, int>, "cannot call set_value on recv_rvalref");
    int val = 0;
    concore::set_value(recv_rvalref{&val}, 10);
    REQUIRE(val == 10);
}
TEST_CASE("can call set_value on a receiver with ref type", "[execution][cpo_receiver]") {
    static_assert(has_set_value<recv_ref&, int>, "cannot call set_value on recv_ref");
    int val = 0;
    recv_ref recv{&val};
    concore::set_value(recv, 10);
    REQUIRE(val == 10);
}
TEST_CASE("can call set_value on a receiver with const ref type", "[execution][cpo_receiver]") {
    static_assert(has_set_value<recv_cref, int>, "cannot call set_value on recv_cref");
    int val = 0;
    concore::set_value(recv_cref{&val}, 10);
    REQUIRE(val == 10);
}

TEST_CASE("can call set_error on a receiver with plain value type", "[execution][cpo_receiver]") {
    static_assert(has_set_error<recv_value, int>, "cannot call set_error on recv_value");
    int val = 0;
    concore::set_error(recv_value{&val}, 10);
    REQUIRE(val == -10);
}
TEST_CASE("can call set_error on a receiver with r-value ref type", "[execution][cpo_receiver]") {
    static_assert(has_set_error<recv_rvalref, int>, "cannot call set_error on recv_rvalref");
    int val = 0;
    concore::set_error(recv_rvalref{&val}, 10);
    REQUIRE(val == -10);
}
TEST_CASE("can call set_error on a receiver with ref type", "[execution][cpo_receiver]") {
    static_assert(has_set_error<recv_ref&, int>, "cannot call set_error on recv_ref");
    int val = 0;
    recv_ref recv{&val};
    concore::set_error(recv, 10);
    REQUIRE(val == -10);
}
TEST_CASE("can call set_error on a receiver with const ref type", "[execution][cpo_receiver]") {
    static_assert(has_set_error<recv_cref, int>, "cannot call set_error on recv_cref");
    int val = 0;
    concore::set_error(recv_cref{&val}, 10);
    REQUIRE(val == -10);
}

TEST_CASE("can call set_done on a receiver with plain value type", "[execution][cpo_receiver]") {
    static_assert(has_set_done<recv_value>, "cannot call set_done on recv_value");
    int val = 0;
    concore::set_done(recv_value{&val});
    REQUIRE(val == INT_MAX);
}
TEST_CASE("can call set_done on a receiver with r-value ref type", "[execution][cpo_receiver]") {
    static_assert(has_set_done<recv_rvalref>, "cannot call set_done on recv_rvalref");
    int val = 0;
    concore::set_done(recv_rvalref{&val});
    REQUIRE(val == INT_MAX);
}
TEST_CASE("can call set_done on a receiver with ref type", "[execution][cpo_receiver]") {
    static_assert(has_set_done<recv_ref&>, "cannot call set_done on recv_ref");
    int val = 0;
    recv_ref recv{&val};
    concore::set_done(recv);
    REQUIRE(val == INT_MAX);
}
TEST_CASE("can call set_done on a receiver with const ref type", "[execution][cpo_receiver]") {
    static_assert(has_set_done<recv_cref>, "cannot call set_done on recv_cref");
    int val = 0;
    concore::set_done(recv_cref{&val});
    REQUIRE(val == INT_MAX);
}

TEST_CASE("set_value can be called through tag_invoke", "[execution][cpo_receiver]") {
    int val = 0;
    tag_invoke(concore::set_value, recv_value{&val}, 10);
    REQUIRE(val == 10);
}
TEST_CASE("set_error can be called through tag_invoke", "[execution][cpo_receiver]") {
    int val = 0;
    tag_invoke(concore::set_error, recv_value{&val}, 10);
    REQUIRE(val == -10);
}
TEST_CASE("set_done can be called through tag_invoke", "[execution][cpo_receiver]") {
    int val = 0;
    tag_invoke(concore::set_done, recv_value{&val});
    REQUIRE(val == INT_MAX);
}

TEST_CASE("tag types can be deduced from set_value, set_error and set_done",
        "[execution][cpo_receiver]") {
    static_assert(std::is_same_v<const set_value_t, decltype(concore::set_value)>, "type mismatch");
    static_assert(std::is_same_v<const set_error_t, decltype(concore::set_error)>, "type mismatch");
    static_assert(std::is_same_v<const set_done_t, decltype(concore::set_done)>, "type mismatch");
}
