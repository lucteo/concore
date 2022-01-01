#include <catch2/catch.hpp>
#include <concore/execution.hpp>

#if CONCORE_USE_CXX2020 && CONCORE_CPP_VERSION >= 20
#include <test_common/receivers_new.hpp>

#include <functional>

using concore::connect_t;
using concore::_p2300::tag_invocable;

template <typename R>
struct op_state {
    int val_;
    R recv_;

    op_state(int val, R&& recv)
        : val_(val)
        , recv_((R &&) recv) {}

    friend void tag_invoke(concore::start_t, op_state& self) noexcept {
        concore::set_value((R &&) self.recv_, self.val_);
    }
};

struct my_sender {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = false;

    int value_{0};

    template <typename R>
    friend op_state<R> tag_invoke(connect_t, my_sender&& s, R&& r) {
        return {s.value_, (R &&) r};
    }
};

TEST_CASE("can call connect on an appropriate types", "[execution][cpo_connect]") {
    auto op = concore::connect(my_sender{10}, expect_value_receiver{10});
    concore::start(op);
    // the receiver will check the received value
}

TEST_CASE("cannot connect sender with invalid receiver", "[execution][cpo_connect]") {
    // REQUIRE_FALSE(tag_invocable<connect_t, my_sender, int>);
    // TODO: this should not work
    // invalid check:
    REQUIRE(tag_invocable<connect_t, my_sender, int>);
}

struct strange_receiver {
    bool* called_;

    friend inline op_state<strange_receiver> tag_invoke(
            connect_t, my_sender, strange_receiver self) {
        *self.called_ = true;
        // NOLINTNEXTLINE
        return {19, std::move(self)};
    }

    friend inline void tag_invoke(concore::set_value_t, strange_receiver, int val) {
        REQUIRE(val == 19);
    }
    friend void tag_invoke(concore::set_done_t, strange_receiver) noexcept {}
    friend void tag_invoke(concore::set_error_t, strange_receiver, std::exception_ptr) noexcept {}
};

TEST_CASE("connect can be defined in the receiver", "[execution][cpo_connect]") {
    static_assert(concore::sender<my_sender>);
    static_assert(concore::receiver<strange_receiver>);
    bool called{false};
    auto op = concore::connect(my_sender{10}, strange_receiver{&called});
    concore::start(op);
    REQUIRE(called);
}

#endif
