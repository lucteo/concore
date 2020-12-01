#include <catch2/catch.hpp>
#include <concore/std/execution.hpp>
#include <concore/std/thread_pool.hpp>

#if CONCORE_CXX_HAS_CONCEPTS

TEST_CASE("thread_pool executor models the executor concept", "[execution][concepts]") {
    auto pool = concore::std_execution::static_thread_pool(2);
    auto ex = pool.executor();
    REQUIRE(concore::std_execution::executor<decltype(ex)>);
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
    REQUIRE_FALSE(concore::std_execution::executor<not_quite_executor>);
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
    REQUIRE(concore::std_execution::executor<NS1::my_executor>);
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
    REQUIRE(concore::std_execution::executor<NS2::my_executor>);
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
inline void tag_invoke(concore::std_execution::execute_t, const my_executor& ex, F&& f) {
    ex((F &&) f);
}
} // namespace NS3

TEST_CASE("struct with tag_invoke extension point models the executor concept",
        "[execution][concepts]") {
    REQUIRE(concore::std_execution::executor<NS3::my_executor>);
}

#endif