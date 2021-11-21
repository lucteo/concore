#include <catch2/catch.hpp>
#include <concore/execution.hpp>
#include <concore/thread_pool.hpp>

#include <functional>

struct my_oper_state_tag_invoke {
    bool started_{false};
};

void tag_invoke(concore::start_t, my_oper_state_tag_invoke& op) { op.started_ = true; }

template <typename O>
void test_start() {
    O oper;
    CHECK_FALSE(oper.started_);
    concore::start(oper);
    CHECK(oper.started_);
}

TEST_CASE("oper object with tag_invoke connect fulfills start CPO", "[execution][cpo_start]") {
    test_start<my_oper_state_tag_invoke>();
}
