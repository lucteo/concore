#include <catch2/catch.hpp>
#include <concore/as_receiver.hpp>
#include <concore/as_invocable.hpp>
#include <concore/as_operation.hpp>
#include <concore/as_sender.hpp>
#include <concore/as_scheduler.hpp>
#include <concore/execution.hpp>

using concore::set_done_t;
using concore::set_error_t;
using concore::set_value_t;

namespace test_models {

struct my_executor {
    friend inline bool operator==(my_executor, my_executor) { return false; }
    friend inline bool operator!=(my_executor, my_executor) { return true; }

    template <typename F>
    void execute(F&& f) const {
        ((F &&) f)();
    }
};
struct my_receiver0 {
    friend void tag_invoke(set_value_t, my_receiver0&&) {}
    friend void tag_invoke(set_done_t, my_receiver0&&) noexcept {}
    friend void tag_invoke(set_error_t, my_receiver0&&, std::exception_ptr) noexcept {}
};
struct my_receiver_int {
    friend void tag_invoke(set_value_t, my_receiver_int&&, int) {}
    friend void tag_invoke(set_done_t, my_receiver_int&&) noexcept {}
    friend void tag_invoke(set_error_t, my_receiver_int&&, std::exception_ptr) noexcept {}
};

struct my_receiver0_ec {
    friend void tag_invoke(set_value_t, my_receiver0_ec&&) {}
    friend void tag_invoke(set_done_t, my_receiver0_ec&&) noexcept {}
    friend void tag_invoke(set_error_t, my_receiver0_ec&&, std::error_code) noexcept {}
};
struct my_receiver_int_ec {
    friend void tag_invoke(set_value_t, my_receiver_int_ec&&, int) {}
    friend void tag_invoke(set_done_t, my_receiver_int_ec&&) noexcept {}
    friend void tag_invoke(set_error_t, my_receiver_int_ec&&, std::error_code) noexcept {}
};

struct my_operation {
    void start() noexcept {}
};
struct my_sender0 {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    my_operation connect(my_receiver0&& r) { return my_operation{}; }
};
struct my_sender_int {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<int>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    my_operation connect(my_receiver_int&& r) { return my_operation{}; }
};
struct my_scheduler {
    friend inline bool operator==(my_scheduler, my_scheduler) { return false; }
    friend inline bool operator!=(my_scheduler, my_scheduler) { return true; }

    my_sender0 schedule() noexcept { return {}; }
};

struct calling_op {
    std::function<void()> op_;
    void start() noexcept { op_(); }
};

struct signal_sender {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = false;

    template <typename R>
    calling_op connect(R& r) {
        auto f = [&r] { concore::set_value(r); };
        return calling_op{std::move(f)};
    }
};

struct test_receiver {
    bool& called_;
    friend void tag_invoke(set_value_t, test_receiver&& self) { self.called_ = true; }
    friend void tag_invoke(set_done_t, test_receiver&&) noexcept {}
    friend void tag_invoke(set_error_t, test_receiver&&, std::exception_ptr) noexcept {}
};

} // namespace test_models

TEST_CASE("as_receiver transforms a functor into a receiver", "[execution][concept_wrappers]") {
    using namespace concore;
    auto f = [] {};
    f();
    static_assert(receiver_of<as_receiver<decltype(f)>>);
}

TEST_CASE("as_receiver passed to a sender will call the given ftor",
        "[execution][concept_wrappers]") {
    using namespace concore;
    using namespace test_models;

    bool called = false;
    auto f = [&called] { called = true; };
    auto recv = as_receiver<decltype(f)>(std::move(f));

    CHECK_FALSE(called);
    concore::submit(signal_sender{}, recv);
    CHECK(called);

    auto void_fun = [] {};
    void_fun();
    static_assert(receiver_of<as_receiver<decltype(void_fun)>>);
}

#if CONCORE_CPP_VERSION >= 17
TEST_CASE("as_invocable transforms a receiver into a functor", "[execution][concept_wrappers]") {
    using namespace concore;
    using namespace test_models;
    using invocalbe_t = as_invocable<my_receiver0>;
    static_assert(std::is_invocable<invocalbe_t>::value, "invalid as_invocable");
}
#endif

struct logging_receiver {
    int* state_;
    bool should_throw_{false};
    friend void tag_invoke(set_value_t, logging_receiver&& self) {
        if (self.should_throw_)
            throw std::logic_error("test");
        *self.state_ = 0;
    }
    friend void tag_invoke(set_done_t, logging_receiver&& self) noexcept { *self.state_ = 1; }
    friend void tag_invoke(set_error_t, logging_receiver&& self, std::exception_ptr) noexcept {
        *self.state_ = 2;
    }
};

TEST_CASE("as_invocable properly calls the right methods in the receiver",
        "[execution][concept_wrappers]") {
    using namespace concore;
    using namespace test_models;

    int state1 = -1;
    int state2 = -1;
    int state3 = -1;
    logging_receiver l1{&state1};
    logging_receiver l2{&state2};
    logging_receiver l3{&state3, true};

    {
        auto f1 = as_invocable<logging_receiver>{l1};
        auto f2 = as_invocable<logging_receiver>{l2};
        auto f3 = as_invocable<logging_receiver>{l3};

        f1();
        // f2();       // don't call
        f3();
    }

    CHECK(state1 == 0); // set_value() called
    CHECK(state2 == 1); // set_done() called
    CHECK(state3 == 2); // set_error() called
}

TEST_CASE("as_operation transforms an executor and a receiver into an operation_state",
        "[execution][concept_wrappers]") {
    using namespace concore;
    using namespace test_models;
    static_assert(operation_state<as_operation<my_executor, my_receiver0>>);
}

TEST_CASE("as_operation produces a good operation", "[execution][concept_wrappers]") {
    using namespace concore;
    using namespace test_models;

    bool called = false;

    test_receiver recv{called};
    auto op = as_operation<my_executor, test_receiver>(my_executor{}, recv);
    CHECK_FALSE(called);
    concore::start(op);
    CHECK(called);
}

TEST_CASE("as_sender transforms an executor into a sender", "[execution][concept_wrappers]") {
    using namespace concore;
    using namespace test_models;
    static_assert(sender_to<as_sender<my_executor>, my_receiver0>);
}

TEST_CASE("as_sender produces a good sender", "[execution][concept_wrappers]") {
    using namespace concore;
    using namespace test_models;

    bool called = false;

    test_receiver recv{called};
    auto snd = as_sender<my_executor>(my_executor{});
    CHECK_FALSE(called);
    concore::submit(snd, recv);
    CHECK(called);
}

TEST_CASE("as_scheduler produces a good scheduler", "[execution][concept_wrappers]") {
    using namespace concore;
    using namespace test_models;

    bool called = false;

    test_receiver recv{called};
    auto sched = as_scheduler<my_executor>(my_executor{});
    CHECK_FALSE(called);
    concore::submit(sched.schedule(), recv);
    static_assert(concore::scheduler<typeof(sched)>, "Type is not a scheduler");
    CHECK(called);
}