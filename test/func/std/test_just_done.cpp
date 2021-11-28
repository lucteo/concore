#include <catch2/catch.hpp>
#include <concore/sender_algo/just_done.hpp>
#include <test_common/task_utils.hpp>
#include <test_common/receivers.hpp>

TEST_CASE("Simple test for just_done", "[sender_algo]") {
    auto op = concore::connect(concore::just_done(), expect_done_receiver{});
    concore::start(op);
    // receiver ensures that set_done() is called
}

TEST_CASE("just_done returns a sender", "[sender_algo]") {
    using t = decltype(concore::just_done());
    static_assert(concore::sender<t>, "concore::just_done must return a sender");
    REQUIRE(concore::sender<t>);
}

template <typename... Ts>
struct type_array {};

TEST_CASE("value types are properly set for just_done", "[sender_algo]") {
    using t = decltype(concore::just_done());

    using et = concore::sender_traits<t>::value_types<type_array, type_array>;
    static_assert(std::is_same<et, type_array<>>::value);
}
TEST_CASE("error types are properly set for just_done", "[sender_algo]") {
    using t = decltype(concore::just_done());

    using et = concore::sender_traits<t>::error_types<type_array>;
    static_assert(std::is_same<et, type_array<>>::value);
}
TEST_CASE("just_done advertises that it can call set_done", "[sender_algo]") {
    using t = decltype(concore::just_done());
    CHECK(concore::sender_traits<t>::sends_done);
}
