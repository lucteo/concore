#include <catch2/catch.hpp>

#include <concore/computation/computation.hpp>
#include <concore/computation/just_value.hpp>

#include <string>

template <typename C, typename R>
void ensure_computation() {
    static_assert(concore::computation::computation<C>, "Given type is not a computation");
    static_assert(std::is_same_v<typename C::value_type, R>, "Invalid value_type in computation");
}

template <typename T>
struct test_value_receiver {
    T* res;
    void set_value(T v) { *res = v; }
    void set_done() { FAIL("set_done() called"); }
    void set_error(std::exception) { FAIL("set_error() called"); }
};

struct test_void_receiver {
    bool* called;
    void set_value() { *called = true; }
    void set_done() { FAIL("set_done() called"); }
    void set_error(std::exception) { FAIL("set_error() called"); }
};

TEST_CASE("just_value(int) is a computation that yields the specified value", "[computation]") {
    auto c = concore::computation::just_value(10);
    ensure_computation<decltype(c), int>();

    int res{0};
    concore::computation::run_with(c, test_value_receiver<int>{&res});
    REQUIRE(res == 10);
}

TEST_CASE("just_value(string) is a computation that yields the specified value", "[computation]") {
    auto c = concore::computation::just_value(std::string{"Hello, world!"});
    ensure_computation<decltype(c), std::string>();

    std::string res;
    concore::computation::run_with(c, test_value_receiver<std::string>{&res});
    REQUIRE(res == "Hello, world!");
}

TEST_CASE("just_value() is a computation that yields nothing", "[computation]") {
    auto c = concore::computation::just_value();
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    REQUIRE(called);
}

TEST_CASE("just_void() is a computation that yields nothing", "[computation]") {
    auto c = concore::computation::just_void();
    ensure_computation<decltype(c), void>();

    bool called{false};
    concore::computation::run_with(c, test_void_receiver{&called});
    REQUIRE(called);
}
