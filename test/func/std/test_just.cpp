#include <catch2/catch.hpp>
#include <concore/execution.hpp>
#include <test_common/receivers_new.hpp>
#include <test_common/type_helpers.hpp>

#if CONCORE_USE_CXX2020 && CONCORE_CPP_VERSION >= 20

namespace ex = concore::execution;

TEST_CASE("Simple test for just", "[sender_algo]") {
    auto o1 = ex::connect(ex::just(1), expect_value_receiver(1));
    ex::start(o1);
    auto o2 = ex::connect(ex::just(2), expect_value_receiver(2));
    ex::start(o2);
    auto o3 = ex::connect(ex::just(3), expect_value_receiver(3));
    ex::start(o3);

    auto o4 =
            ex::connect(ex::just(std::string("this")), expect_value_receiver(std::string("this")));
    ex::start(o4);
    auto o5 =
            ex::connect(ex::just(std::string("that")), expect_value_receiver(std::string("that")));
    ex::start(o5);
}

TEST_CASE("just returns a sender", "[sender_algo]") {
    using t = decltype(ex::just(1));
    static_assert(ex::sender<t>, "ex::just must return a sender");
    REQUIRE(ex::sender<t>);
}

TEST_CASE("just can handle multiple values", "[sender_algo]") {
    bool executed{false};
    auto f = [&](int x, double d) {
        CHECK(x == 3);
        CHECK(d == 0.14);
        executed = true;
    };
    auto op = ex::connect(ex::just(3, 0.14), make_fun_receiver(std::move(f)));
    ex::start(op);
    CHECK(executed);
}

TEST_CASE("value types are properly set for just", "[sender_algo]") {
    check_val_types<type_array<type_array<int>>>(ex::just(1));
    check_val_types<type_array<type_array<double>>>(ex::just(3.14));
    check_val_types<type_array<type_array<std::string>>>(ex::just(std::string{}));

    check_val_types<type_array<type_array<int, double>>>(ex::just(1, 3.14));
    check_val_types<type_array<type_array<int, double, std::string>>>(
            ex::just(1, 3.14, std::string{}));
}

TEST_CASE("error types are properly set for just", "[sender_algo]") {
    check_err_type<type_array<std::exception_ptr>>(ex::just(1));
}

TEST_CASE("just cannot call set_done", "[sender_algo]") { check_sends_done<false>(ex::just(1)); }

TEST_CASE("just works with value type", "[sender_algo]") {
    auto snd = ex::just(std::string{"hello"});

    // Check reported type
    check_val_types<type_array<type_array<std::string>>>(snd);

    // Check received value
    std::string res;
    typecat cat{typecat::undefined};
    auto op = ex::connect(std::move(snd), typecat_receiver<std::string>{&res, &cat});
    ex::start(op);
    CHECK(res == "hello");
    CHECK(cat == typecat::rvalref);
}
TEST_CASE("just works with ref type", "[sender_algo]") {
    std::string original{"hello"};
    auto snd = ex::just(original);

    // Check reported type
    check_val_types<type_array<type_array<std::string>>>(snd);

    // Check received value
    std::string res;
    typecat cat{typecat::undefined};
    auto op = ex::connect(std::move(snd), typecat_receiver<std::string>{&res, &cat});
    ex::start(op);
    CHECK(res == original);
    CHECK(cat == typecat::rvalref);
}
TEST_CASE("just works with const-ref type", "[sender_algo]") {
    const std::string original{"hello"};
    auto snd = ex::just(original);

    // Check reported type
    check_val_types<type_array<type_array<std::string>>>(snd);

    // Check received value
    std::string res;
    typecat cat{typecat::undefined};
    auto op = ex::connect(std::move(snd), typecat_receiver<std::string>{&res, &cat});
    ex::start(op);
    CHECK(res == original);
    CHECK(cat == typecat::rvalref);
}

#endif