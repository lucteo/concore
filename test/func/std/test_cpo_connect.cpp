#include <catch2/catch.hpp>
#include <concore/execution.hpp>
#include <concore/thread_pool.hpp>

#include <functional>

using concore::set_done_t;
using concore::set_error_t;
using concore::set_value_t;

struct my_receiver {
    friend inline bool operator==(my_receiver, my_receiver) { return false; }
    friend inline bool operator!=(my_receiver, my_receiver) { return true; }

    friend void tag_invoke(set_value_t, my_receiver& self, int x) { self.value_ = x; }
    friend void tag_invoke(set_done_t, my_receiver& self) noexcept { self.value_ = -1; }
    friend void tag_invoke(set_error_t, my_receiver& self, std::error_code) noexcept {
        self.value_ = -2;
    }

    friend void tag_invoke(set_value_t, my_receiver&& self, int x) { self.value_ = x; }
    friend void tag_invoke(set_done_t, my_receiver&& self) noexcept { self.value_ = -1; }
    friend void tag_invoke(set_error_t, my_receiver&& self, std::error_code) noexcept {
        self.value_ = -2;
    }

    int value_{0};
};

struct op_state {
    std::function<void()> fun_;

    op_state(std::function<void()> f)
        : fun_(std::move(f)) {}

    void start() noexcept { fun_(); }
};

struct my_sender_in {
    friend inline bool operator==(my_sender_in, my_sender_in) { return false; }
    friend inline bool operator!=(my_sender_in, my_sender_in) { return true; }

    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    int value_{0};
    op_state connect(my_receiver& r) {
        int val = value_;
        return op_state([val, &r] { concore::set_value(r, val); });
    }
};

struct my_sender_ext {
    friend inline bool operator==(my_sender_ext, my_sender_ext) { return false; }
    friend inline bool operator!=(my_sender_ext, my_sender_ext) { return true; }

    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    int value_{0};
    friend op_state connect(my_sender_ext& s, my_receiver& r) {
        int val = s.value_;
        return op_state([val, &r] { concore::set_value(r, val); });
    }
};

struct my_sender_tag_invoke {
    friend inline bool operator==(my_sender_tag_invoke, my_sender_tag_invoke) { return false; }
    friend inline bool operator!=(my_sender_tag_invoke, my_sender_tag_invoke) { return true; }

    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    int value_{0};
};

op_state tag_invoke(concore::connect_t, my_sender_tag_invoke& s, my_receiver& r) {
    int val = s.value_;
    return op_state([val, &r] { concore::set_value(r, val); });
}

template <typename S>
void test_connect() {
    my_receiver recv;
    S snd{10};
    auto op = concore::connect(snd, recv);
    CHECK(recv.value_ == 0);
    op.start();
    CHECK(recv.value_ == 10);
}

TEST_CASE("sender object with inner method fulfills connect CPO", "[execution][cpo_connect]") {
    test_connect<my_sender_in>();
}

TEST_CASE("sender object with external function fulfills connect CPO", "[execution][cpo_connect]") {
    test_connect<my_sender_ext>();
}

TEST_CASE(
        "sender object with tag_invoke connect fulfills connect CPO", "[execution][cpo_connect]") {
    test_connect<my_sender_tag_invoke>();
}
