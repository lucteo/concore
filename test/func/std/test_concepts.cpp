#include <catch2/catch.hpp>
#include <concore/execution.hpp>
#include <concore/thread_pool.hpp>
#include <test_common/receivers.hpp>

TEST_CASE("thread_pool executor models the executor concept", "[execution][concepts]") {
    auto pool = concore::static_thread_pool(2);
    auto ex = pool.executor();
    REQUIRE(concore::executor<decltype(ex)>);
    static_cast<void>(ex);
}

struct not_quite_executor {
    using shape_type = size_t;
    using index_type = size_t;

    not_quite_executor() noexcept = default;
    not_quite_executor(const not_quite_executor& r) noexcept = default;
    not_quite_executor& operator=(const not_quite_executor& r) noexcept = default;
    not_quite_executor(not_quite_executor&& r) noexcept = default;
    not_quite_executor& operator=(not_quite_executor&& r) noexcept = default;
    ~not_quite_executor() = default;

    friend inline bool operator==(not_quite_executor, not_quite_executor) { return false; }
    friend inline bool operator!=(not_quite_executor, not_quite_executor) { return true; }

    bool running_in_this_thread() const noexcept { return false; }

    template <typename F>
    void operator()(F&& f) const {
        ((F &&) f)();
    }
};

TEST_CASE("not all classes model the executor concept", "[execution][concepts]") {
    REQUIRE_FALSE(concore::executor<not_quite_executor>);
}

namespace NS1 {
struct my_executor {
    friend inline bool operator==(my_executor, my_executor) { return false; }
    friend inline bool operator!=(my_executor, my_executor) { return true; }

    template <typename F>
    void operator()(F&& f) const {
        ((F &&) f)();
    }

    template <typename F>
    friend inline void execute(const my_executor& ex, F&& f) {
        ex((F &&) f);
    }
};
} // namespace NS1

TEST_CASE("structure with friend 'execute' models the executor concept", "[execution][concepts]") {
    REQUIRE(concore::executor<NS1::my_executor>);
}

namespace NS2 {
struct my_executor {
    friend inline bool operator==(my_executor, my_executor) { return false; }
    friend inline bool operator!=(my_executor, my_executor) { return true; }

    template <typename F>
    void operator()(F&& f) const {
        ((F &&) f)();
    }
};

template <typename F>
inline void execute(const my_executor& ex, F&& f) {
    ex((F &&) f);
}
} // namespace NS2

TEST_CASE("struct with external 'execute' models the executor concept", "[execution][concepts]") {
    REQUIRE(concore::executor<NS2::my_executor>);
}

namespace NS3 {
struct my_executor {
    friend inline bool operator==(my_executor, my_executor) { return false; }
    friend inline bool operator!=(my_executor, my_executor) { return true; }

    template <typename F>
    void operator()(F&& f) const {
        ((F &&) f)();
    }
};

template <typename F>
inline void tag_invoke(concore::execute_t, const my_executor& ex, F&& f) {
    ex((F &&) f);
}
} // namespace NS3

TEST_CASE("struct with tag_invoke extension point models the executor concept",
        "[execution][concepts]") {
    REQUIRE(concore::executor<NS3::my_executor>);
}

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
    friend void tag_invoke(concore::start_t, my_operation&) noexcept {}
};

struct my_sender0 {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    friend my_operation tag_invoke(concore::connect_t, my_sender0, empty_recv::recv0&& r) {
        return my_operation{};
    }
};
struct my_sender_int {
    template <template <class...> class Tuple, template <class...> class Variant>
    using value_types = Variant<Tuple<int>>;
    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;
    static constexpr bool sends_done = true;

    friend my_operation tag_invoke(concore::connect_t, my_sender_int, empty_recv::recv_int&& r) {
        return my_operation{};
    }
};
struct my_scheduler {
    friend inline bool operator==(my_scheduler, my_scheduler) { return false; }
    friend inline bool operator!=(my_scheduler, my_scheduler) { return true; }

    friend my_sender0 tag_invoke(concore::schedule_t, my_scheduler) noexcept { return {}; }
};
} // namespace test_models

void void_fun() {}

TEST_CASE("executor concept is properly modeled", "[execution][concepts]") {
    using namespace concore;
    using namespace test_models;

    using void_fun_t = decltype(&void_fun);

    REQUIRE(executor<my_executor>);
    REQUIRE(executor_of<my_executor, void_fun_t>);
    REQUIRE_FALSE(receiver<my_executor>);
    REQUIRE_FALSE(receiver_of<my_executor>);
    REQUIRE_FALSE(operation_state<my_executor>);
    REQUIRE_FALSE(sender<my_executor>);
    REQUIRE_FALSE(sender_to<my_executor, empty_recv::recv0>);
    REQUIRE_FALSE(typed_sender<my_executor>);
    REQUIRE_FALSE(scheduler<my_executor>);
}

TEST_CASE("sender types satisfy the sender concept", "[execution][concepts]") {
    using namespace concore;
    using namespace test_models;

    REQUIRE(sender<my_sender0>);
    REQUIRE(sender<my_sender_int>);
}

TEST_CASE("sender types satisfy the typed_sender concept", "[execution][concepts]") {
    using namespace concore;
    using namespace test_models;

    REQUIRE(typed_sender<my_sender0>);
    REQUIRE(typed_sender<my_sender_int>);
}

TEST_CASE("sender & receiver types satisfy the sender_to concept", "[execution][concepts]") {
    using namespace concore;
    using namespace empty_recv;
    using namespace test_models;

    REQUIRE(sender_to<my_sender0, recv0>);
    REQUIRE(sender_to<my_sender_int, recv_int>);
}

TEST_CASE("not all combinations of senders & receivers satisfy the sender_to concept",
        "[execution][concepts]") {
    using namespace concore;
    using namespace empty_recv;
    using namespace test_models;

    REQUIRE_FALSE(sender_to<my_sender0, recv_int>);
    REQUIRE_FALSE(sender_to<my_sender0, recv0_ec>);
    REQUIRE_FALSE(sender_to<my_sender0, recv_int_ec>);
    REQUIRE_FALSE(sender_to<my_sender_int, recv0>);
    REQUIRE_FALSE(sender_to<my_sender_int, recv0_ec>);
    REQUIRE_FALSE(sender_to<my_sender_int, recv_int_ec>);
}
