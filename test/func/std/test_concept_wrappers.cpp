#include <catch2/catch.hpp>
#include <concore/as_receiver.hpp>
#include <concore/as_invocable.hpp>
#include <concore/as_operation.hpp>
#include <concore/as_sender.hpp>
#include <concore/execution.hpp>

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
    friend inline bool operator==(my_receiver0, my_receiver0) { return false; }
    friend inline bool operator!=(my_receiver0, my_receiver0) { return true; }

    void set_value() {}
    void set_done() noexcept {}
    void set_error(std::exception_ptr) noexcept {}
};
struct my_receiver_int {
    friend inline bool operator==(my_receiver_int, my_receiver_int) { return false; }
    friend inline bool operator!=(my_receiver_int, my_receiver_int) { return true; }

    void set_value(int) {}
    void set_done() noexcept {}
    void set_error(std::exception_ptr) noexcept {}
};

struct my_receiver0_ec {
    friend inline bool operator==(my_receiver0_ec, my_receiver0_ec) { return false; }
    friend inline bool operator!=(my_receiver0_ec, my_receiver0_ec) { return true; }

    void set_value() {}
    void set_done() noexcept {}
    void set_error(std::error_code) noexcept {}
};
struct my_receiver_int_ec {
    friend inline bool operator==(my_receiver_int_ec, my_receiver_int_ec) { return false; }
    friend inline bool operator!=(my_receiver_int_ec, my_receiver_int_ec) { return true; }

    void set_value(int) {}
    void set_done() noexcept {}
    void set_error(std::error_code) noexcept {}
};

struct my_operation {
    friend inline bool operator==(my_operation, my_operation) { return false; }
    friend inline bool operator!=(my_operation, my_operation) { return true; }

    void start() noexcept {}
};
struct my_sender0 {
    friend inline bool operator==(my_sender0, my_sender0) { return false; }
    friend inline bool operator!=(my_sender0, my_sender0) { return true; }

    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    my_operation connect(my_receiver0&& r) { return my_operation{}; }
};
struct my_sender_int {
    friend inline bool operator==(my_sender_int, my_sender_int) { return false; }
    friend inline bool operator!=(my_sender_int, my_sender_int) { return true; }

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
    friend inline bool operator==(signal_sender, signal_sender) { return false; }
    friend inline bool operator!=(signal_sender, signal_sender) { return true; }

    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = false;

    template <typename R>
    calling_op connect(R& r) {
        auto f = [&r] { r.set_value(); };
        return calling_op{std::move(f)};
    }
};

struct test_receiver {
    bool& called_;
    void set_value() { called_ = true; }
    void set_done() noexcept {}
    void set_error(std::exception_ptr) noexcept {}
};

} // namespace test_models

#if CONCORE_CXX_HAS_CONCEPTS
TEST_CASE("as_receiver transforms a functor into a receiver", "[execution][concept_wrappers]") {
    using namespace concore;
    static_assert(receiver_of<as_receiver<decltype([] {})>>);
}
#endif

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

#if CONCORE_CXX_HAS_CONCEPTS
    static_assert(receiver_of<as_receiver<decltype([] {})>>);
#endif
}

#if CONCORE_CXX_HAS_CONCEPTS
TEST_CASE("as_invocable transforms a receiver into a functor", "[execution][concept_wrappers]") {
    using namespace concore;
    using namespace test_models;
    static_assert(std::invocable<as_invocable<my_receiver0>>);
}
#endif

TEST_CASE("as_invocable properly calls the right methods in the receiver",
        "[execution][concept_wrappers]") {
    using namespace concore;
    using namespace test_models;

    struct logging_receiver {
        bool should_throw_{false};
        int state_{-1};
        void set_value() {
            if (should_throw_)
                throw std::logic_error("test");
            state_ = 0;
        }
        void set_done() noexcept { state_ = 1; }
        void set_error(std::exception_ptr) noexcept { state_ = 2; }
    };

    logging_receiver l1;
    logging_receiver l2;
    logging_receiver l3{true};

    {
        auto f1 = as_invocable<logging_receiver>{l1};
        auto f2 = as_invocable<logging_receiver>{l2};
        auto f3 = as_invocable<logging_receiver>{l3};

        f1();
        // f2();       // don't call
        f3();
    }

    CHECK(l1.state_ == 0); // set_value() called
    CHECK(l2.state_ == 1); // set_done() called
    CHECK(l3.state_ == 2); // set_error() called
}

#if CONCORE_CXX_HAS_CONCEPTS
TEST_CASE("as_operation transforms an executor and a receiver into an operation_state",
        "[execution][concept_wrappers]") {
    using namespace concore;
    using namespace test_models;
    static_assert(operation_state<as_operation<my_executor, my_receiver0>>);
}
#endif

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
