#include <catch2/catch.hpp>
#include <concore/sender_algo/just.hpp>
#include <concore/detail/sender_helpers.hpp>
#include <test_common/task_utils.hpp>
#include <test_common/receivers.hpp>

TEST_CASE("Simple test for just", "[sender_algo]") {
    auto o1 = concore::connect(concore::just(1), expect_value_receiver(1));
    concore::start(o1);
    auto o2 = concore::connect(concore::just(2), expect_value_receiver(2));
    concore::start(o2);
    auto o3 = concore::connect(concore::just(3), expect_value_receiver(3));
    concore::start(o3);

    auto o4 = concore::connect(
            concore::just(std::string("this")), expect_value_receiver(std::string("this")));
    concore::start(o4);
    auto o5 = concore::connect(
            concore::just(std::string("that")), expect_value_receiver(std::string("that")));
    concore::start(o5);
}

TEST_CASE("just returns a sender", "[sender_algo]") {
    using t = decltype(concore::just(1));
    static_assert(concore::sender<t>, "concore::just must return a sender");
    REQUIRE(concore::sender<t>);
}

TEST_CASE("just has proper return type", "[sender_algo]") {
    using st_int = decltype(concore::just(1));
    using st_double = decltype(concore::just(3.14));
    using st_str = decltype(concore::just(std::string{}));

    using st_int_double = decltype(concore::just(1, 3.14));
    using st_int_double_str = decltype(concore::just(1, 3.14, std::string{}));

    using concore::detail::sender_single_return_type;
    static_assert(std::is_same_v<sender_single_return_type<st_int>, int>,
            "Improper return type for `just`");
    static_assert(std::is_same_v<sender_single_return_type<st_double>, double>,
            "Improper return type for `just`");
    static_assert(std::is_same_v<sender_single_return_type<st_str>, std::string>,
            "Improper return type for `just`");
    static_assert(std::is_same_v<sender_single_return_type<st_int_double>, std::tuple<int, double>>,
            "Improper return type for `just`");
    static_assert(std::is_same_v<sender_single_return_type<st_int_double_str>,
                          std::tuple<int, double, std::string>>,
            "Improper return type for `just`");
}

TEST_CASE("just can handle multiple values", "[sender_algo]") {
    bool executed{false};
    auto f = [&](int x, double d) {
        CHECK(x == 3);
        CHECK(d == 0.14);
        executed = true;
    };
    auto op = concore::connect(concore::just(3, 0.14), make_fun_receiver(std::move(f)));
    concore::start(op);
    CHECK(executed);
}

template <typename... Ts>
struct type_array {};

TEST_CASE("value types are properly set for just", "[sender_algo]") {
    using t1 = decltype(concore::just(1));
    using t2 = decltype(concore::just(3, 0.14));
    using t3 = decltype(concore::just(3, 0.14, std::string{"pi"}));

    using vt1 = concore::sender_traits<t1>::value_types<type_array, type_array>;
    using vt2 = concore::sender_traits<t2>::value_types<type_array, type_array>;
    using vt3 = concore::sender_traits<t3>::value_types<type_array, type_array>;
    static_assert(std::is_same<vt1, type_array<type_array<int>>>::value);
    static_assert(std::is_same<vt2, type_array<type_array<int, double>>>::value);
    static_assert(std::is_same<vt3, type_array<type_array<int, double, std::string>>>::value);
}

TEST_CASE("error types are properly set for just", "[sender_algo]") {
    using t1 = decltype(concore::just(1));

    using vt1 = concore::sender_traits<t1>::error_types<type_array>;
    static_assert(std::is_same<vt1, type_array<std::exception_ptr>>::value);
}

TEST_CASE("just cannot call set_done", "[sender_algo]") {
    using t1 = decltype(concore::just(1));
    CHECK_FALSE(concore::sender_traits<t1>::sends_done);
}

TEST_CASE("just works with value type", "[sender_algo]") {
    auto snd = concore::just<std::string>(std::string{"hello"});

    // Check reported type
    using S = decltype(snd);
    using value_type = concore::sender_traits<S>::value_types<type_array, type_array>;
    static_assert(std::is_same<value_type, type_array<type_array<std::string>>>::value);

    // Check received value
    std::string res;
    typecat cat{typecat::undefined};
    auto op = concore::connect(std::move(snd), typecat_receiver<std::string>{&res, &cat});
    concore::start(op);
    CHECK(res == "hello");
    CHECK(cat == typecat::rvalref);
}
TEST_CASE("just works with ref type", "[sender_algo]") {
    std::string original{"hello"};
    auto snd = concore::just<std::string&>(original);

    // Check reported type
    using S = decltype(snd);
    using value_type = concore::sender_traits<S>::value_types<type_array, type_array>;
    static_assert(std::is_same<value_type, type_array<type_array<std::string&>>>::value);

    // Check received value
    std::string res;
    typecat cat{typecat::undefined};
    auto op = concore::connect(std::move(snd), typecat_receiver<std::string>{&res, &cat});
    concore::start(op);
    CHECK(res == original);
    CHECK(cat == typecat::ref);
}
TEST_CASE("just works with const-ref type", "[sender_algo]") {
    std::string original{"hello"};
    auto snd = concore::just<const std::string&>(original);

    // Check reported type
    using S = decltype(snd);
    using value_type = concore::sender_traits<S>::value_types<type_array, type_array>;
    static_assert(std::is_same<value_type, type_array<type_array<const std::string&>>>::value);

    // Check received value
    std::string res;
    typecat cat{typecat::undefined};
    auto op = concore::connect(std::move(snd), typecat_receiver<std::string>{&res, &cat});
    concore::start(op);
    CHECK(res == original);
    CHECK(cat == typecat::cref);
}
