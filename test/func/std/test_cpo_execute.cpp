#include <catch2/catch.hpp>
#include <concore/execution.hpp>
#include <concore/thread_pool.hpp>

template <typename Ex>
bool can_call(const Ex& ex) {
    bool called = false;
    auto f = [&] { called = true; };
    concore::execute(ex, std::move(f));
    return called;
}

namespace NS1 {
struct my_executor {
    friend inline bool operator==(my_executor, my_executor) { return false; }
    friend inline bool operator!=(my_executor, my_executor) { return true; }

    template <typename F>
    void execute(F&& f) const {
        ((F &&) f)();
    }
};
} // namespace NS1

TEST_CASE("execute method counts as CPO", "[execution][cpo_execute]") {
    REQUIRE(can_call(NS1::my_executor{}));
}

namespace NS2 {
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
} // namespace NS2

TEST_CASE("friend execute fun counts as CPO", "[execution][cpo_execute]") {
    REQUIRE(can_call(NS2::my_executor{}));
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
inline void execute(const my_executor& ex, F&& f) {
    ex((F &&) f);
}
} // namespace NS3

TEST_CASE("external execute fun counts as CPO", "[execution][cpo_execute]") {
    REQUIRE(can_call(NS3::my_executor{}));
}

namespace NS4 {
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
} // namespace NS4

TEST_CASE("external tag_invoke(execute_t, ...) fun counts as CPO", "[execution][cpo_execute]") {
    REQUIRE(can_call(NS4::my_executor{}));
}

namespace NS5 {
struct my_executor {
    friend inline bool operator==(my_executor, my_executor) { return false; }
    friend inline bool operator!=(my_executor, my_executor) { return true; }

    mutable int fun_called{-1};

    template <typename F>
    void execute(F&& f) const {
        fun_called = 0;
        ((F &&) f)();
    }

    template <typename F>
    friend inline void execute(const my_executor& ex, F&& f) {
        ex.fun_called = 1;
        ex((F &&) f);
    }
};

template <typename F>
inline void tag_invoke(concore::execute_t, const my_executor& ex, F&& f) {
    ex.fun_called = 2;
    ((F &&) f)();
}
} // namespace NS5

TEST_CASE("when multiple execute CPOs are defined, take the one with tag_invoke",
        "[execution][cpo_execute]") {
    NS5::my_executor ex{};
    REQUIRE(can_call(ex));
    REQUIRE(ex.fun_called == 2);
}

TEST_CASE("can call global execute on thread_pool executor", "[execution][cpo_execute]") {
    bool called = false;
    {
        concore::static_thread_pool pool{2};
        auto f = [&] { called = true; };
        concore::execute(pool.executor(), std::move(f));
        pool.wait();
    }
    REQUIRE(called);
}
