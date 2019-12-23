#include <catch2/catch.hpp>

TEST_CASE("Smoke test") {
    SECTION("a first section") { REQUIRE(true); }
    SECTION("the second section") { REQUIRE_FALSE(false); }
}
