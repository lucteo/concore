#include <catch2/catch.hpp>
#include <concore/execution.hpp>
#include <test_common/receivers_new.hpp>
#include <test_common/type_helpers.hpp>

#if CONCORE_USE_CXX2020 && CONCORE_CPP_VERSION >= 20

TEST_CASE("Simple test for just_error", "[sender_algo]") {
    auto op = concore::connect(concore::just_error(std::exception_ptr{}), expect_error_receiver{});
    concore::start(op);
    // receiver ensures that set_error() is called
}

TEST_CASE("just_error returns a sender", "[sender_algo]") {
    using t = decltype(concore::just_error(std::exception_ptr{}));
    static_assert(concore::sender<t>, "concore::just_error must return a sender");
}

TEST_CASE("just_error returns a typed sender", "[sender_algo]") {
    using t = decltype(concore::just_error(std::exception_ptr{}));
    static_assert(concore::typed_sender<t>, "concore::just_error must return a typed_sender");
}

TEST_CASE("error types are properly set for just_error<int>", "[sender_algo]") {
    // check_err_type<type_array<int, std::exception_ptr>>(concore::just_error(1));
    // TODO: we should also expect std::exception_ptr here
    // incorrect test:
    check_err_type<type_array<int>>(concore::just_error(1));
}
TEST_CASE("error types are properly set for just_error<exception_ptr>", "[sender_algo]") {
    // we should not get std::exception_ptr twice
    check_err_type<type_array<std::exception_ptr>>(concore::just_error(std::exception_ptr()));
}

TEST_CASE("value types are properly set for just_error", "[sender_algo]") {
    // there is no variant of calling `set_value(recv)`
    check_val_types<type_array<>>(concore::just_error(1));
}
TEST_CASE("just_error cannot call set_done", "[sender_algo]") {
    check_sends_done<false>(concore::just_error(1));
}

TEST_CASE("just_error removes cv qualifier for the given type", "[sender_algo]") {
    std::string str{"hello"};
    const std::string& crefstr = str;
    auto snd = concore::just_error(crefstr);
    // check_err_type<type_array<std::string, std::exception_ptr>>(snd);
    // TODO: 1) std:exception_ptr must also be returned
    // TODO: 2) don't return const reference here
    // incorrect test:
    check_err_type<type_array<const std::string&>>(snd);
}

#endif
