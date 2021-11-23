#include <catch2/catch.hpp>
#include <concore/_cpo/_cpo_schedule.hpp>

using concore::schedule_t;
using concore::detail::cpo_schedule::has_schedule;

struct my_sender {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    bool from_scheduler_{false};
};

struct my_scheduler {
    friend my_sender tag_invoke(concore::schedule_t, my_scheduler) { return my_sender{true}; }
};

TEST_CASE("can call schedule on an appropriate type", "[execution][cpo_schedule]") {
    static_assert(has_schedule<my_scheduler>, "invalid scheduler type");
    my_scheduler sched;
    auto snd = concore::schedule(sched);
    CHECK(snd.from_scheduler_);
}
