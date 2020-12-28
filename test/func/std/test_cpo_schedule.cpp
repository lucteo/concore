#include <catch2/catch.hpp>
#include <concore/std/execution.hpp>

struct my_sender {
    friend inline bool operator==(my_sender, my_sender) { return false; }
    friend inline bool operator!=(my_sender, my_sender) { return true; }

    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    bool from_scheduler_{false};
};

struct my_scheduler_in {
    my_sender schedule() { return my_sender{true}; }
};

struct my_scheduler_ext {
    friend my_sender schedule(my_scheduler_ext& op) { return my_sender{true}; }
};

struct my_scheduler_tag_invoke {};

my_sender tag_invoke(concore::std_execution::schedule_t, my_scheduler_tag_invoke& sched) {
    return my_sender{true};
}

template <typename Sched>
void test_schedule() {
    Sched sched;
    auto snd = concore::std_execution::schedule(sched);
    CHECK(snd.from_scheduler_);
}

TEST_CASE("scheduler with inner method fulfills schedule CPO", "[execution][cpo_schedule]") {
    test_schedule<my_scheduler_in>();
}

TEST_CASE("scheduler with external function fulfills schedule CPO", "[execution][cpo_schedule]") {
    test_schedule<my_scheduler_ext>();
}

TEST_CASE("scheduler with tag_invoke connect fulfills schedule CPO", "[execution][cpo_schedule]") {
    test_schedule<my_scheduler_tag_invoke>();
}
