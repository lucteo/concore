#include <catch2/catch.hpp>
#include <concore/_concepts/_concept_operation_state.hpp>

using concore::operation_state;
using concore::start_t;

struct op_except {
    friend void tag_invoke(start_t, op_except) {}
};
struct op_noexcept {
    friend void tag_invoke(start_t, op_noexcept) noexcept {}
};

TEST_CASE("type with start CPO that throws is not an operation_state", "[execution][concepts]") {
    REQUIRE(!operation_state<op_except>);
}
TEST_CASE("type with start CPO noexcept is an operation_state", "[execution][concepts]") {
    REQUIRE(operation_state<op_noexcept>);
}

TEST_CASE("reference type is not an operation_state", "[execution][concepts]") {
    REQUIRE(!operation_state<op_noexcept&>);
}
TEST_CASE("pointer type is not an operation_state", "[execution][concepts]") {
    REQUIRE(!operation_state<op_noexcept*>);
}