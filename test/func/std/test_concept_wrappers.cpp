#include <catch2/catch.hpp>
#include <concore/as_receiver.hpp>
#include <concore/as_invocable.hpp>
#include <concore/execution.hpp>
#include <test_common/receivers.hpp>

namespace test_models {

struct my_executor {
    friend inline bool operator==(my_executor, my_executor) { return false; }
    friend inline bool operator!=(my_executor, my_executor) { return true; }

    template <typename F>
    void execute(F&& f) const {
        ((F &&) f)();
    }
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

    my_operation connect(empty_recv::recv0&& r) { return my_operation{}; }
};
struct my_sender_int {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<int>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    my_operation connect(empty_recv::recv_int&& r) { return my_operation{}; }
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
    using invocalbe_t = as_invocable<empty_recv::recv0>;
    static_assert(std::is_invocable<invocalbe_t>::value, "invalid as_invocable");
}
#endif

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
