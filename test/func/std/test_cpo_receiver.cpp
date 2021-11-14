#include <catch2/catch.hpp>
#include <concore/execution.hpp>
#include <test_common/receivers.hpp>

TEST_CASE("can call set_value on a void receiver", "[execution][cpo_receiver]") {
    concore::set_value(expect_void_receiver{});
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
