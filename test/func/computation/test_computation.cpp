#include <catch2/catch.hpp>

#include <concore/computation/computation.hpp>

#include <string>

template <typename C, typename R>
void ensure_computation() {
    static_assert(concore::computation::computation<C>, "Given type is not a computation");
    static_assert(std::is_same_v<typename C::value_type, R>, "Invalid value_type in computation");
}

template <typename C>
void ensure_not_computation() {
    static_assert(!concore::computation::computation<C>,
            "Given type is a computation, when we expect it not to be");
}

template <typename R>
struct simple_computation {
    using value_type = R;

    template <typename Recv>
    void run_with(Recv recv) noexcept {
        recv.set_done();
    }
};

struct bad_computation1 {
    template <typename Recv>
    void run_with(Recv recv) noexcept {
        recv.set_done();
    }
};

struct bad_computation2 {
    using value_type = int;
};

struct comp_val {
    using value_type = int;

    template <typename Recv>
    void run_with(Recv r) {
        ((Recv &&) r).set_value(10);
    }
};

struct comp_with_tag_invoke {
    using value_type = int;
};
template <typename Recv>
void tag_invoke(concore::computation::run_with_t, comp_with_tag_invoke, Recv r) {
    r.set_value(11);
}

struct comp_with_outer_fun {
    using value_type = int;
};

template <typename Recv>
void run_with(comp_with_outer_fun, Recv r) {
    r.set_value(12);
}

struct test_int_receiver {
    int* res;
    void set_value(int v) { *res = v; }
    void set_done() { FAIL("set_done() called"); }
    void set_error(std::exception) { FAIL("set_error() called"); }
};

TEST_CASE("a simple type that matches the computation concept", "[computation]") {
    // Check that some simple computation objects match the concept
    ensure_computation<simple_computation<void>, void>();
    ensure_computation<simple_computation<int>, int>();
    ensure_computation<simple_computation<std::string>, std::string>();

    // Check a few classes that do not match the concept
    ensure_not_computation<bad_computation1>();
    ensure_not_computation<bad_computation2>();

    using namespace concore::computation;
    using namespace concore::computation::detail;
    using C = simple_computation<void>;
    using R = typename C::value_type;
    using Recv = receiver_archetype<void>;
    // static_assert(cpo_run_with::has_run_with<bad_computation2, int>, "test");
    static_assert(std::is_void_v<computation_value_type<C>>, "test");
    static_assert(std::is_void_v<R>, "test");
    static_assert(std::is_same_v<Recv, receiver_archetype<void>>, "test");
    static_assert(detail::cpo_run_with::has_run_with<C, Recv>, "test2");
    static_assert(detail::cpo_run_with::meets_inner_fun<C, Recv>, "test3");
}

TEST_CASE("some types do not match computation concept", "[computation]") {
    ensure_not_computation<bad_computation1>();
    ensure_not_computation<bad_computation2>();
}

TEST_CASE("computation using inner method CPO", "[computation]") {
    ensure_computation<comp_val, int>();

    int res{0};
    concore::computation::run_with(comp_val{}, test_int_receiver{&res});
    REQUIRE(res == 10);
}
TEST_CASE("computation using tag_invoke CPO", "[computation]") {
    ensure_computation<comp_with_tag_invoke, int>();

    int res{0};
    concore::computation::run_with(comp_with_tag_invoke{}, test_int_receiver{&res});
    REQUIRE(res == 11);
}
TEST_CASE("computation using outer function CPO", "[computation]") {
    ensure_computation<comp_with_outer_fun, int>();

    int res{0};
    concore::computation::run_with(comp_with_outer_fun{}, test_int_receiver{&res});
    REQUIRE(res == 12);
}
