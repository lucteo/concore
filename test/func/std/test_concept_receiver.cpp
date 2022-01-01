#include <catch2/catch.hpp>
#include <concore/execution.hpp>

#if CONCORE_USE_CXX2020 && CONCORE_CPP_VERSION >= 20

#include <test_common/receivers_new.hpp>

using concore::receiver;
using concore::receiver_of;
using concore::set_done_t;
using concore::set_error_t;
using concore::set_value_t;

struct recv_no_set_value {
    friend void tag_invoke(set_done_t, recv_no_set_value) noexcept {}
    friend void tag_invoke(set_error_t, recv_no_set_value, std::exception_ptr) noexcept {}
};

struct recv_set_value_except {
    friend void tag_invoke(set_value_t, recv_set_value_except) {}
    friend void tag_invoke(set_done_t, recv_set_value_except) noexcept {}
    friend void tag_invoke(set_error_t, recv_set_value_except, std::exception_ptr) noexcept {}
};
struct recv_set_value_noexcept {
    friend void tag_invoke(set_value_t, recv_set_value_noexcept) noexcept {}
    friend void tag_invoke(set_done_t, recv_set_value_noexcept) noexcept {}
    friend void tag_invoke(set_error_t, recv_set_value_noexcept, std::exception_ptr) noexcept {}
};

struct recv_set_error_except {
    friend void tag_invoke(set_value_t, recv_set_error_except) noexcept {}
    friend void tag_invoke(set_done_t, recv_set_error_except) noexcept {}
    friend void tag_invoke(set_error_t, recv_set_error_except, std::exception_ptr) {
        throw std::logic_error{"err"};
    }
};
struct recv_set_done_except {
    friend void tag_invoke(set_value_t, recv_set_done_except) noexcept {}
    friend void tag_invoke(set_done_t, recv_set_done_except) { throw std::logic_error{"err"}; }
    friend void tag_invoke(set_error_t, recv_set_done_except, std::exception_ptr) noexcept {}
};

struct recv_non_movable {
    recv_non_movable() = default;
    ~recv_non_movable() = default;
    recv_non_movable(recv_non_movable&&) = delete;
    recv_non_movable& operator=(recv_non_movable&&) = delete;
    recv_non_movable(const recv_non_movable&) = default;
    recv_non_movable& operator=(const recv_non_movable&) = default;

    friend void tag_invoke(set_value_t, recv_non_movable) noexcept {}
    friend void tag_invoke(set_done_t, recv_non_movable) noexcept {}
    friend void tag_invoke(set_error_t, recv_non_movable, std::exception_ptr) noexcept {}
};

TEST_CASE("receiver types satisfy the receiver concept", "[execution][concepts]") {
    using namespace empty_recv;

    REQUIRE(receiver<recv0>);
    REQUIRE(receiver<recv_int>);
    REQUIRE(receiver<recv0_ec>);
    REQUIRE(receiver<recv_int_ec>);
    REQUIRE(receiver<recv0_ec, std::error_code>);
    REQUIRE(receiver<recv_int_ec, std::error_code>);
    REQUIRE(receiver<expect_void_receiver>);
    REQUIRE(receiver<expect_void_receiver_ex>);
    REQUIRE(receiver<expect_value_receiver<int>>);
    REQUIRE(receiver<expect_value_receiver<double>>);
    REQUIRE(receiver<expect_done_receiver>);
    REQUIRE(receiver<expect_done_receiver_ex>);
    REQUIRE(receiver<expect_error_receiver>);
    REQUIRE(receiver<expect_error_receiver_ex>);
    REQUIRE(receiver<logging_receiver>);
}

TEST_CASE("receiver types satisfy the receiver_of concept", "[execution][concepts]") {
    using namespace empty_recv;

    REQUIRE(receiver_of<recv0>);
    REQUIRE(receiver_of<recv_int, int>);
    REQUIRE(receiver_of<recv0_ec>);
    REQUIRE(receiver_of<recv_int_ec, int>);
    REQUIRE(receiver_of<expect_void_receiver>);
    REQUIRE(receiver_of<expect_void_receiver_ex>);
    REQUIRE(receiver_of<expect_value_receiver<int>, int>);
    REQUIRE(receiver_of<expect_value_receiver<double>, double>);
    REQUIRE(receiver_of<expect_done_receiver, char>);
    REQUIRE(receiver_of<expect_done_receiver_ex, char>);
    REQUIRE(receiver_of<expect_error_receiver, char>);
    REQUIRE(receiver_of<expect_error_receiver_ex, char>);
    REQUIRE(receiver_of<logging_receiver>);
}

TEST_CASE("receiver type w/o set_value models receiver but not receiver_of",
        "[execution][concepts]") {
    REQUIRE(receiver<recv_no_set_value>);
    REQUIRE(!receiver_of<recv_no_set_value>);
}

TEST_CASE("type with set_value that throws is a receiver", "[execution][concepts]") {
    REQUIRE(receiver<recv_set_value_except>);
    REQUIRE(receiver_of<recv_set_value_except>);
}
TEST_CASE("type with set_value noexcept is a receiver", "[execution][concepts]") {
    REQUIRE(receiver<recv_set_value_noexcept>);
    REQUIRE(receiver_of<recv_set_value_noexcept>);
}
TEST_CASE("type with throwing set_error is not a receiver", "[execution][concepts]") {
    REQUIRE(!receiver<recv_set_error_except>);
    REQUIRE(!receiver_of<recv_set_error_except>);
}
TEST_CASE("type with throwing set_done is not a receiver", "[execution][concepts]") {
    REQUIRE(!receiver<recv_set_done_except>);
    REQUIRE(!receiver_of<recv_set_done_except>);
}

TEST_CASE("non-movable type is not a receiver", "[execution][concepts]") {
    REQUIRE(!receiver<recv_non_movable>);
    REQUIRE(!receiver_of<recv_non_movable>);
}

#endif