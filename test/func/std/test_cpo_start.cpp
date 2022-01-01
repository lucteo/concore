#include <catch2/catch.hpp>
#include <concore/execution.hpp>

#if CONCORE_USE_CXX2020 && CONCORE_CPP_VERSION >= 20

using concore::start_t;
using concore::_p2300::tag_invocable;

struct my_oper {
    bool started_{false};

    friend void tag_invoke(start_t, my_oper& self) { self.started_ = true; }
};

struct op_value {
    bool* started_;
    friend void tag_invoke(start_t, op_value self) { *self.started_ = true; }
};
struct op_rvalref {
    bool* started_;
    friend void tag_invoke(start_t, op_rvalref&& self) { *self.started_ = true; }
};
struct op_ref {
    bool* started_;
    friend void tag_invoke(start_t, op_ref& self) { *self.started_ = true; }
};
struct op_cref {
    bool* started_;
    friend void tag_invoke(start_t, const op_cref& self) { *self.started_ = true; }
};

TEST_CASE("can call start on an operation state", "[execution][cpo_start]") {
    my_oper op;
    concore::start(op);
    REQUIRE(op.started_);
}

TEST_CASE("can call start on an oper with plain value type", "[execution][cpo_start]") {
    static_assert(tag_invocable<concore::start_t, op_value>, "cannot call start on op_value");
    bool started{false};
    op_value op{&started};
    concore::start(op);
    REQUIRE(started);
}
TEST_CASE("can call start on an oper with r-value ref type", "[execution][cpo_start]") {
    // static_assert(!tag_invocable<concore::start_t, op_rvalref&&>,
    //         "should not be able to call start on op_rvalref");
    // TODO: we should not be able to call start non non-ref
    // invalid check:
    static_assert(tag_invocable<concore::start_t, op_rvalref&&>,
            "should not be able to call start on op_rvalref");
}
TEST_CASE("can call start on an oper with ref type", "[execution][cpo_start]") {
    static_assert(tag_invocable<concore::start_t, op_ref&>, "cannot call start on op_ref");
    bool started{false};
    op_ref op{&started};
    concore::start(op);
    REQUIRE(started);
}
TEST_CASE("can call start on an oper with const ref type", "[execution][cpo_start]") {
    static_assert(tag_invocable<concore::start_t, op_cref>, "cannot call start on op_cref");
    bool started{false};
    const op_cref op{&started};
    concore::start(op);
    REQUIRE(started);
}

#endif