#include <catch2/catch.hpp>
#include <concore/inline_executor.hpp>
#include <concore/execution.hpp>

#include <atomic>
#include <thread>

TEST_CASE("inline_executor is copyable") {
    auto e1 = concore::inline_executor{};
    auto e2 = concore::inline_executor{};
    // cppcheck-suppress redundantInitialization
    // cppcheck-suppress unreadVariable
    e2 = e1;
}

TEST_CASE("inline_executor executes a task") {
    std::atomic<int> val{0};
    auto f = [&val]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        val = 1;
    };
    concore::execute(concore::inline_executor{}, f);
    REQUIRE(val.load() == 1);
}
