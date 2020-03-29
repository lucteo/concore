#include <catch2/catch.hpp>
#include <concore/spawn.hpp>

#include <thread>
#include <atomic>
#include <chrono>

using namespace std::chrono_literals;

TEST_CASE("can wait on a spawn task (w/o worker)", "[wait]") {
    std::atomic<bool> executed{false};
    auto ftor = [&]() {
        std::this_thread::sleep_for(3ms);
        executed = true;
    };
    concore::spawn_and_wait(ftor);
    REQUIRE(executed);
}

TEST_CASE("can wait on a spawn task (inside worker)", "[wait]") {
    std::atomic<bool> executed{false};

    auto outer_ftor = [&executed]() {
        concore::spawn_and_wait([&executed]() {
            std::this_thread::sleep_for(3ms);
            executed = true;
        });
    };
    concore::spawn_and_wait(outer_ftor);
    REQUIRE(executed);
}

TEST_CASE("can wait on multiple tasks to finish", "[wait]") {
    std::atomic<int> counter{0};

    auto outer_ftor = [&counter]() {
        auto ftor = [&counter]() {
            std::this_thread::sleep_for(3ms);
            counter++;
        };
        concore::spawn_and_wait({ftor, ftor, ftor});
    };
    concore::spawn_and_wait(outer_ftor);
    REQUIRE(counter);
}
