#include <catch2/catch.hpp>
#include <concore/any_executor.hpp>
#include <concore/inline_executor.hpp>

#include <atomic>
#include <thread>

TEST_CASE("any_executor is copyable", "[any_executor]") {
    auto e1 = concore::any_executor{};
    auto e2 = concore::any_executor{};
    // cppcheck-suppress redundantInitialization
    // cppcheck-suppress unreadVariable
    e2 = e1;
}

TEST_CASE("any_executor executes a task", "[any_executor]") {
    std::atomic<int> val{0};
    auto f = [&val]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        val = 1;
    };
    concore::inline_executor base;
    auto my_exec = concore::any_executor{base};
    REQUIRE(my_exec);
    concore::execute(my_exec, f);
    REQUIRE(val.load() == 1);
}

TEST_CASE("any_executor can be null", "[any_executor]") {
    concore::any_executor e1;
    concore::any_executor e2{nullptr};
    concore::any_executor e3{concore::inline_executor{}};
    concore::any_executor e4{concore::inline_executor{}};

    e3 = nullptr;
    e4 = e3;

    REQUIRE_FALSE(e1);
    REQUIRE_FALSE(e2);
    REQUIRE_FALSE(e3);
    REQUIRE_FALSE(e4);
}

TEST_CASE("any_executor can be compared", "[any_executor]") {
    concore::any_executor e1;
    concore::any_executor e2{nullptr};
    concore::any_executor e3{concore::inline_executor{}};
    concore::any_executor e4;
    e4 = concore::inline_executor{};

    REQUIRE((e1 == e2));
    REQUIRE((e1 != e3));
    REQUIRE((e1 != e4));
    REQUIRE((e2 != e3));
    REQUIRE((e2 != e4));
    REQUIRE((e3 == e4));
}

TEST_CASE("any_executor can expose the type_info for the wrapped executor", "[any_executor]") {
    concore::any_executor e1;
    concore::any_executor e2{nullptr};
    concore::any_executor e3{concore::inline_executor{}};
    concore::any_executor e4;
    e4 = concore::inline_executor{};

    REQUIRE(e1.target_type() == typeid(std::nullptr_t));
    REQUIRE(e2.target_type() == typeid(std::nullptr_t));
    REQUIRE(e3.target_type() == typeid(concore::inline_executor));
    REQUIRE(e4.target_type() == typeid(concore::inline_executor));
}

struct throwing_executor {
    template <typename F>
    void execute(F f) const {
        throw std::logic_error("err");
    }
    void execute(concore::task t) const noexcept {
        auto cont = t.get_continuation();
        if (cont) {
            try {
                throw std::logic_error("err");
            } catch (...) {
                cont(std::current_exception());
            }
        }
    }

    friend inline bool operator==(throwing_executor, throwing_executor) { return true; }
    friend inline bool operator!=(throwing_executor, throwing_executor) { return false; }
};

TEST_CASE("any_executor doesn't throw when enqueuing tasks", "[any_executor]") {
    concore::any_executor e1{throwing_executor{}};

    std::atomic<int> counter{0};
    std::atomic<int> counter_err{0};
    auto task_fun = [&counter] { counter++; };
    auto cont_fun = [&counter_err](std::exception_ptr) { counter_err++; };
    concore::task t{task_fun, {}, cont_fun};

    e1.execute(std::move(t));

    REQUIRE(counter.load() == 0);
    REQUIRE(counter_err.load() == 1);
}
