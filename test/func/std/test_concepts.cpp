#include <catch2/catch.hpp>
#include <concore/execution.hpp>
#include <concore/thread_pool.hpp>
#include <concore/execution.hpp>

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

    template <typename F>
    void bulk_execute(F&& f, size_t n) const {
        for (size_t i = 0; i < n; i++)
            ((F &&) f)(i);
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
    // REQUIRE_FALSE(sender<my_executor>);
    // REQUIRE_FALSE(sender_to<my_executor, test_models::my_receiver0>);
    // REQUIRE_FALSE(typed_sender<my_executor>);
    // REQUIRE_FALSE(scheduler<my_executor>);
}

TEST_CASE("receiver types satisfy the receiver concept", "[execution][concepts]") {
    using namespace concore;
    using namespace test_models;

    REQUIRE(receiver<my_receiver0>);
    REQUIRE(receiver<my_receiver_int>);
    REQUIRE(receiver<my_receiver0_ec, std::error_code>);
    REQUIRE(receiver<my_receiver_int_ec, std::error_code>);
}

TEST_CASE("receiver types satisfy the receiver_of concept", "[execution][concepts]") {
    using namespace concore;
    using namespace test_models;

    REQUIRE(receiver_of<my_receiver0>);
    REQUIRE(receiver_of<my_receiver_int, std::exception_ptr, int>);
    REQUIRE(receiver_of<my_receiver0_ec, std::error_code>);
    REQUIRE(receiver_of<my_receiver_int_ec, std::error_code, int>);
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
    using namespace test_models;

    REQUIRE(sender_to<my_sender0, my_receiver0>);
    REQUIRE(sender_to<my_sender_int, my_receiver_int>);
}

TEST_CASE("not all combinations of senders & receivers satisfy the sender_to concept",
        "[execution][concepts]") {
    using namespace concore;
    using namespace test_models;

    REQUIRE_FALSE(sender_to<my_sender0, my_receiver_int>);
    REQUIRE_FALSE(sender_to<my_sender0, my_receiver0_ec>);
    REQUIRE_FALSE(sender_to<my_sender0, my_receiver_int_ec>);
    REQUIRE_FALSE(sender_to<my_sender_int, my_receiver0>);
    REQUIRE_FALSE(sender_to<my_sender_int, my_receiver0_ec>);
    REQUIRE_FALSE(sender_to<my_sender_int, my_receiver_int_ec>);
}

TEST_CASE("scheduler types satisfy the scheduler concept", "[execution][concepts]") {
    using namespace concore;
    using namespace test_models;

    REQUIRE(scheduler<my_scheduler>);
    auto snd = concore::schedule(my_scheduler{});
    REQUIRE(sender<decltype(snd)>);
    static_cast<void>(snd);
}

TEST_CASE("other types don't satisfy the scheduler concept", "[execution][concepts]") {
    using namespace concore;
    using namespace test_models;

    REQUIRE_FALSE(scheduler<my_executor>);
    REQUIRE_FALSE(scheduler<my_sender0>);
    REQUIRE_FALSE(scheduler<my_receiver0>);
    REQUIRE_FALSE(scheduler<my_operation>);
}

TEST_CASE("operation types satisfy the operation_state concept", "[execution][concepts]") {
    using namespace concore;
    using namespace test_models;

    REQUIRE(operation_state<my_operation>);
}
