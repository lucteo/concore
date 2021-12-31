#include <catch2/catch.hpp>
#include <concore/execution.hpp>
#include <test_common/receivers_new.hpp>
#include <test_common/type_helpers.hpp>

#if CONCORE_USE_CXX2020 && CONCORE_CPP_VERSION >= 20

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

TEST_CASE("just_done returns a typed sender", "[sender_algo]") {
    using t = decltype(concore::just_done());
    static_assert(concore::typed_sender<t>, "concore::just_done must return a typed_sender");
}

TEST_CASE("value types are properly set for just_done", "[sender_algo]") {
    check_val_types<type_array<>>(concore::just_done());
}
TEST_CASE("error types are properly set for just_done", "[sender_algo]") {
    // no errors sent by just_done
    check_err_type<type_array<>>(concore::just_done());
}
TEST_CASE("just_done advertises that it can call set_done", "[sender_algo]") {
    check_sends_done<true>(concore::just_done());
}

#endif
