#include <catch2/catch.hpp>
#include <concore/execution_old.hpp>
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
