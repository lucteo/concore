#include <catch2/catch.hpp>
#include <concore/sender_algo/just_error.hpp>
#include <test_common/task_utils.hpp>
#include <test_common/receivers.hpp>

TEST_CASE("Simple test for just_error", "[sender_algo]") {
    auto op = concore::connect(concore::just_error(std::exception_ptr{}), expect_error_receiver{});
    concore::start(op);
    // receiver ensures that set_error() is called
}

TEST_CASE("just_error returns a sender", "[sender_algo]") {
    using t = decltype(concore::just_error(std::exception_ptr{}));
    static_assert(concore::sender<t>, "concore::just_error must return a sender");
    REQUIRE(concore::sender<t>);
}

template <typename... Ts>
struct type_array {};

TEST_CASE("error types are properly set for just_error<int>", "[sender_algo]") {
    using t = decltype(concore::just_error(1));

    using et = concore::sender_traits<t>::error_types<type_array>;
    static_assert(std::is_same<et, type_array<int, std::exception_ptr>>::value);
}
TEST_CASE("error types are properly set for just_error<exception_ptr>", "[sender_algo]") {
    using t = decltype(concore::just_error(std::exception_ptr()));

    using et = concore::sender_traits<t>::error_types<type_array>;
    static_assert(std::is_same<et, type_array<std::exception_ptr, std::exception_ptr>>::value);
}

TEST_CASE("value types are properly set for just_error", "[sender_algo]") {
    using t = decltype(concore::just_error(1));

    using et = concore::sender_traits<t>::value_types<type_array, type_array>;
    static_assert(std::is_same<et, type_array<>>::value);
}
TEST_CASE("just_error cannot call set_done", "[sender_algo]") {
    using t = decltype(concore::just_error(1));
    CHECK_FALSE(concore::sender_traits<t>::sends_done);
}

TEST_CASE("just_error removes cv qualifier for the given type", "[sender_algo]") {
    std::string str{"hello"};
    const std::string& crefstr = str;
    auto snd = concore::just_error(crefstr);
    using t = decltype(snd);

    using et = concore::sender_traits<t>::error_types<type_array>;
    CHECK(std::is_same<et, type_array<std::string, std::exception_ptr>>::value);
}