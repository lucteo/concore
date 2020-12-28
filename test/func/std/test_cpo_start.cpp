#include <catch2/catch.hpp>
#include <concore/std/execution.hpp>
#include <concore/std/thread_pool.hpp>

#include <functional>

struct my_oper_state_in {
    bool started_{false};
    void start() { started_ = true; }
};

struct my_oper_state_ext {
    bool started_{false};
    friend void start(my_oper_state_ext& op) { op.started_ = true; }
};

struct my_oper_state_tag_invoke {
    bool started_{false};
};

void tag_invoke(concore::std_execution::start_t, my_oper_state_tag_invoke& op) {
    op.started_ = true;
}

template <typename O>
void test_start() {
    O oper;
    CHECK_FALSE(oper.started_);
    concore::std_execution::start(oper);
    CHECK(oper.started_);
}

TEST_CASE("oper object with inner method fulfills start CPO", "[execution][cpo_start]") {
    test_start<my_oper_state_in>();
}

TEST_CASE("oper object with external function fulfills start CPO", "[execution][cpo_start]") {
    test_start<my_oper_state_ext>();
}

TEST_CASE("oper object with tag_invoke connect fulfills start CPO", "[execution][cpo_start]") {
    test_start<my_oper_state_tag_invoke>();
}
