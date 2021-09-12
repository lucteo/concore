#include <catch2/catch.hpp>
#include <concore/execution.hpp>

struct my_sender {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    bool from_scheduler_{false};
};

struct my_scheduler_tag_invoke {};

my_sender tag_invoke(concore::schedule_t, my_scheduler_tag_invoke& sched) {
    return my_sender{true};
}

template <typename Sched>
void test_schedule() {
    Sched sched;
    auto snd = concore::schedule(sched);
    CHECK(snd.from_scheduler_);
}

TEST_CASE("scheduler with tag_invoke connect fulfills schedule CPO", "[execution][cpo_schedule]") {
    test_schedule<my_scheduler_tag_invoke>();
}
