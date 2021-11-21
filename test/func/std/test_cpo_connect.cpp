#include <catch2/catch.hpp>
#include <concore/execution.hpp>
#include <concore/thread_pool.hpp>
#include <test_common/receivers.hpp>

#include <functional>

using concore::set_done_t;
using concore::set_error_t;
using concore::set_value_t;

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

struct my_sender_tag_invoke {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    int value_{0};
};

template <typename R>
op_state<R> tag_invoke(concore::connect_t, my_sender_tag_invoke&& s, R&& r) {
    return {s.value_, (R &&) r};
}

TEST_CASE(
        "sender object with tag_invoke connect fulfills connect CPO", "[execution][cpo_connect]") {
    auto op = concore::connect(my_sender_tag_invoke{10}, expect_value_receiver{10});
    concore::start(op);
    // the receiver will check the received value
}
