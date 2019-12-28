#include <catch2/catch.hpp>
#include <concore/immediate_executor.hpp>

#include <atomic>
#include <thread>

TEST_CASE("immediate_executor is copyable") {
    auto e1 = concore::immediate_executor;
    auto e2 = concore::immediate_executor;
    e2 = e1;
}

TEST_CASE("immediate_executor executes a task") {
    std::atomic<int> val{0};
    auto f = [&val]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        val = 1;
    };
    concore::immediate_executor(f);
    REQUIRE(val.load() == 1);
}
