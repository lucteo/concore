#include <concore/profiling.hpp>

#if CONCORE_ENABLE_PROFILING
#include <catch2/catch.hpp>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

TEST_CASE("test just for waiting -- ensure profiling writes all the data") {
    {
        CONCORE_PROFILING_SCOPE_N("ending the program...");
        std::this_thread::sleep_for(3s);
    }
}

#endif