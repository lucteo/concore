#include <catch2/catch.hpp>
#include <concore/_concepts/_concept_scheduler.hpp>

using concore::schedule_t;
using concore::scheduler;
using concore::sender;

struct my_sender {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    bool from_scheduler_{false};
};

struct my_scheduler {
    friend my_sender tag_invoke(schedule_t, my_scheduler) { return my_sender{true}; }

    friend bool operator==(my_scheduler, my_scheduler) noexcept { return true; }
    friend bool operator!=(my_scheduler, my_scheduler) noexcept { return false; }
};

struct no_schedule_cpo {
    friend void tag_invoke(int, no_schedule_cpo) {}
};

struct my_scheduler_except {
    friend my_sender tag_invoke(schedule_t, my_scheduler_except) {
        throw std::logic_error("err");
        return my_sender{true};
    }

    friend bool operator==(my_scheduler_except, my_scheduler_except) noexcept { return true; }
    friend bool operator!=(my_scheduler_except, my_scheduler_except) noexcept { return false; }
};

struct my_scheduler_noequal {
    friend my_sender tag_invoke(schedule_t, my_scheduler_noequal) { return my_sender{true}; }
};

TEST_CASE("type with schedule CPO models scheduler", "[execution][concepts]") {
    REQUIRE(scheduler<my_scheduler>);
    // NOLINTNEXTLINE
    auto snd = concore::schedule(my_scheduler{});
    REQUIRE(sender<decltype(snd)>);
}
TEST_CASE("type without schedule CPO doesn't model scheduler", "[execution][concepts]") {
    REQUIRE(!scheduler<no_schedule_cpo>);
}
TEST_CASE("type with schedule that throws is a scheduler", "[execution][concepts]") {
    REQUIRE(scheduler<my_scheduler_except>);
    // NOLINTNEXTLINE
    auto snd = concore::schedule(my_scheduler{});
    REQUIRE(sender<decltype(snd)>);
}

#if CONCORE_CXX_HAS_CONCEPTS
TEST_CASE("type w/o equality operations do not model scheduler", "[execution][concepts]") {
    REQUIRE(!scheduler<my_scheduler_noequal>);
}
#endif