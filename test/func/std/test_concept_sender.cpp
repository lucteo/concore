#include <catch2/catch.hpp>
#include <concore/_concepts/_concepts_sender.hpp>
#include <concore/_concepts/_concept_sender_to.hpp>
#include <concore/_cpo/_cpo_connect.hpp>
#include <concore/_cpo/_cpo_start.hpp>
#include <test_common/receivers.hpp>

using concore::connect_t;
using concore::sender;
using concore::sender_of;
using concore::sender_to;
using concore::typed_sender;

struct oper {
    friend void tag_invoke(concore::start_t, oper&) noexcept {}
};

struct empty_sender : concore::sender_base {};

TEST_CASE("type deriving from sender_base, w/o start, is a sender", "[execution][concepts]") {
    REQUIRE(sender<empty_sender>);
}
TEST_CASE("type deriving from sender_base, w/o start, doesn't model typed_sender",
        "[execution][concepts]") {
    REQUIRE(!typed_sender<empty_sender>);
}
TEST_CASE("type deriving from sender_base, w/o start, doesn't model sender_to",
        "[execution][concepts]") {
    REQUIRE(!sender_to<empty_sender, empty_recv::recv0>);
}
TEST_CASE("type deriving from sender_base, w/o start, doesn't model sender_of",
        "[execution][concepts]") {
    REQUIRE(!sender_of<empty_sender>);
    REQUIRE(!sender_of<empty_sender, int>);
}

struct simple_sender : concore::sender_base {
    template <typename R>
    friend oper tag_invoke(connect_t, simple_sender, R) {
        return {};
    }
};

TEST_CASE("type deriving from sender_base models sender and sender_to", "[execution][concepts]") {
    REQUIRE(sender<simple_sender>);
    REQUIRE(sender_to<simple_sender, empty_recv::recv0>);
}
TEST_CASE("type deriving from sender_base, doesn't model typed_sender", "[execution][concepts]") {
    REQUIRE(!typed_sender<simple_sender>);
}
TEST_CASE("type deriving from sender_base, doesn't model sender_of", "[execution][concepts]") {
    REQUIRE(!sender_of<simple_sender>);
    REQUIRE(!sender_of<simple_sender, int>);
}

struct my_sender0 {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    friend oper tag_invoke(connect_t, my_sender0, empty_recv::recv0&& r) { return {}; }
};
TEST_CASE("type w/ proper types, is a sender & typed_sender", "[execution][concepts]") {
    REQUIRE(sender<my_sender0>);
    REQUIRE(typed_sender<my_sender0>);
}
TEST_CASE("sender that accepts a void sender models sender_to the given sender",
        "[execution][concepts]") {
    REQUIRE(sender_to<my_sender0, empty_recv::recv0>);
}
TEST_CASE("sender of void, models sender_of<>", "[execution][concepts]") {
    REQUIRE(!sender_of<my_sender0>);
}
TEST_CASE("sender of void, doesn't model sender_of<int>", "[execution][concepts]") {
    REQUIRE(!sender_of<my_sender0, int>);
}

struct my_sender_int {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<int>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    friend oper tag_invoke(connect_t, my_sender_int, empty_recv::recv_int&& r) { return {}; }
};
TEST_CASE("my_sender_int is a sender & typed_sender", "[execution][concepts]") {
    REQUIRE(sender<my_sender_int>);
    REQUIRE(typed_sender<my_sender_int>);
}
TEST_CASE("sender that accepts an int receiver models sender_to the given receiver",
        "[execution][concepts]") {
    REQUIRE(sender_to<my_sender_int, empty_recv::recv_int>);
}
TEST_CASE("sender of int, models sender_of<int>", "[execution][concepts]") {
    REQUIRE(!sender_of<my_sender_int, int>);
}
TEST_CASE("sender of int, doesn't model sender_of<double>", "[execution][concepts]") {
    REQUIRE(!sender_of<my_sender_int, double>);
}
TEST_CASE("sender of int, doesn't model sender_of<short>", "[execution][concepts]") {
    REQUIRE(!sender_of<my_sender_int, short>);
}
TEST_CASE("sender of int, doesn't model sender_of<>", "[execution][concepts]") {
    REQUIRE(!sender_of<my_sender_int>);
}

TEST_CASE("not all combinations of senders & receivers satisfy the sender_to concept",
        "[execution][concepts]") {
    REQUIRE_FALSE(sender_to<my_sender0, empty_recv::recv_int>);
    REQUIRE_FALSE(sender_to<my_sender0, empty_recv::recv0_ec>);
    REQUIRE_FALSE(sender_to<my_sender0, empty_recv::recv_int_ec>);
    REQUIRE_FALSE(sender_to<my_sender_int, empty_recv::recv0>);
    REQUIRE_FALSE(sender_to<my_sender_int, empty_recv::recv0_ec>);
    REQUIRE_FALSE(sender_to<my_sender_int, empty_recv::recv_int_ec>);
}

TEST_CASE("can apply sender traits to invalid sender", "[execution][concepts]") {
    REQUIRE(sizeof(concore::sender_traits<empty_sender>) <= sizeof(int));
}

TEST_CASE("can apply sender traits to senders deriving from sender_base", "[execution][concepts]") {
    REQUIRE(sizeof(concore::sender_traits<simple_sender>) <= sizeof(int));
}

template <typename... Ts>
struct types_array {};

TEST_CASE(
        "can query sender traits for a typed sender that sends nothing", "[execution][concepts]") {
    using S = my_sender0;
    using value_types = concore::sender_traits<S>::value_types<types_array, types_array>;
    static_assert(std::is_same<value_types, types_array<types_array<>>>::value);

    using error_types = concore::sender_traits<S>::error_types<types_array>;
    static_assert(std::is_same<error_types, types_array<std::exception_ptr>>::value);

    CHECK(concore::sender_traits<S>::sends_done);
}
TEST_CASE("can query sender traits for a typed sender that sends int", "[execution][concepts]") {
    using S = my_sender_int;
    using value_types = concore::sender_traits<S>::value_types<types_array, types_array>;
    static_assert(std::is_same<value_types, types_array<types_array<int>>>::value);

    using error_types = concore::sender_traits<S>::error_types<types_array>;
    static_assert(std::is_same<error_types, types_array<std::exception_ptr>>::value);

    CHECK(concore::sender_traits<S>::sends_done);
}

struct mutli_sender {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<int, double>, Tuple<short, long>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = false;

    friend oper tag_invoke(connect_t, mutli_sender, empty_recv::recv_int&& r) { return {}; }
};
TEST_CASE("check sender traits for sender that advertises multiple sets of values",
        "[execution][concepts]") {
    using S = mutli_sender;
    using value_types = concore::sender_traits<S>::value_types<types_array, types_array>;
    static_assert(std::is_same<value_types,
            types_array<types_array<int, double>, types_array<short, long>>>::value);

    using error_types = concore::sender_traits<S>::error_types<types_array>;
    static_assert(std::is_same<error_types, types_array<std::exception_ptr>>::value);

    CHECK_FALSE(concore::sender_traits<S>::sends_done);
}

struct ec_sender {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr, int>;
    static constexpr bool sends_done = false;

    friend oper tag_invoke(connect_t, ec_sender, empty_recv::recv_int&& r) { return {}; }
};
TEST_CASE(
        "check sender traits for sender that also supports error codes", "[execution][concepts]") {
    using S = ec_sender;
    using value_types = concore::sender_traits<S>::value_types<types_array, types_array>;
    static_assert(std::is_same<value_types, types_array<types_array<>>>::value);

    using error_types = concore::sender_traits<S>::error_types<types_array>;
    static_assert(std::is_same<error_types, types_array<std::exception_ptr, int>>::value);

    CHECK_FALSE(concore::sender_traits<S>::sends_done);
}
