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
struct my_operation {
    friend inline bool operator==(my_operation, my_operation) { return false; }
    friend inline bool operator!=(my_operation, my_operation) { return true; }

    void start() noexcept {}
};
struct my_sender0 {
    friend inline bool operator==(my_sender0, my_sender0) { return false; }
    friend inline bool operator!=(my_sender0, my_sender0) { return true; }

    template <typename R>
    my_operation connect(R&& r) {
        return my_operation{};
    }
};
struct my_sender_int {
    friend inline bool operator==(my_sender_int, my_sender_int) { return false; }
    friend inline bool operator!=(my_sender_int, my_sender_int) { return true; }

    template <typename R>
    my_operation connect(R&& r) {
        return my_operation{};
    }
};
struct my_scheduler {
    friend inline bool operator==(my_scheduler, my_scheduler) { return false; }
    friend inline bool operator!=(my_scheduler, my_scheduler) { return true; }

    void schedule() noexcept {}
};
} // namespace test_models

TEST_CASE("executor concept is properly modeled", "[execution][concepts]") {
    using namespace concore::std_execution;
    using my_type = test_models::my_executor;

    using void_fun = decltype([]() {});

    REQUIRE(executor<my_type>);
    REQUIRE(executor_of<my_type, void_fun>);
    REQUIRE_FALSE(receiver<my_type>);
    REQUIRE_FALSE(receiver_of<my_type>);
    REQUIRE_FALSE(operation_state<my_type>);
    // REQUIRE_FALSE(sender<my_type>);
    // REQUIRE_FALSE(sender_to<my_type, test_models::my_receiver0>);
    REQUIRE_FALSE(typed_sender<my_type>);
    // REQUIRE_FALSE(scheduler<my_type>);
}

#endif