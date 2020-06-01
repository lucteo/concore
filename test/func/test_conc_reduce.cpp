
#include <catch2/catch.hpp>
#include <concore/conc_reduce.hpp>
#include "arb_partition_hints.hpp"
#include <concore/integral_iterator.hpp>
#include <concore/profiling.hpp>
#include <concore/init.hpp>

#include <thread>
#include <atomic>
#include <chrono>
#include <forward_list>

using namespace std::chrono_literals;

using concore::integral_iterator;

TEST_CASE("conc_reduce is equivalent to std::accumulate (vector)", "[conc_reduce]") {
    PROPERTY([](concore::partition_hints hints, std::vector<short> v) {
        // Do the iterations in parallel
        auto op = [](int id, int i) -> int { return id + i; };
        auto reduction = [](int lhs, int rhs) -> int { return lhs + rhs; };
        auto res = concore::conc_reduce(v.begin(), v.end(), 0, op, reduction, hints);

        // Run std::accumulate
        auto expected = std::accumulate(v.begin(), v.end(), 0, op);

        // check the result
        RC_ASSERT(res == expected);
    });
}
TEST_CASE("conc_reduce is equivalent to std::accumulate (forward_list)", "[conc_reduce]") {
    PROPERTY([](concore::partition_hints hints, std::forward_list<int> v) {
        // Do the iterations in parallel
        auto op = [](int id, int i) -> int { return id + i; };
        auto reduction = [](int lhs, int rhs) -> int { return lhs + rhs; };
        auto res = concore::conc_reduce(v.begin(), v.end(), 0, op, reduction, hints);

        // Run std::accumulate
        auto expected = std::accumulate(v.begin(), v.end(), 0, op);

        // check the result
        RC_ASSERT(res == expected);
    });
}
TEST_CASE("conc_reduce can be canceled", "[conc_reduce]") {
    PROPERTY([](concore::partition_hints hints) {
        auto grp = concore::task_group::create();
        std::atomic<bool> first = true;

        // Do the iterations in parallel
        auto op = [&](int id, int i) -> int {
            // Cancel the tasks in the group -- just do it in one task
            if (first.load()) {
                grp.cancel();
                first = false;
            }
            // While we are not canceled, spin around
            for (int i = 0; i < 1000; i++) {
                if (concore::task_group::is_current_task_cancelled())
                    return 0;
                std::this_thread::sleep_for(1us);
            }
            RC_FAIL("Tasks are not properly canceled");
            return id + i;
        };
        auto reduction = [](int lhs, int rhs) -> int { return lhs + rhs; };
        concore::conc_reduce(
                integral_iterator(0), integral_iterator(1000), 0, op, reduction, grp, hints);
    });
}
TEST_CASE("conc_reduce forwards the exceptions in the binary operation", "[conc_reduce]") {
    PROPERTY([](concore::partition_hints hints) {
        constexpr int num_iter = 100;
        int res = 0;
        int not_expected = (num_iter - 1) * num_iter / 2;

        auto op = [](int id, int i) -> int {
            if (i == 0)
                throw std::runtime_error("some error");
            return id + i;
        };
        auto reduction = [](int lhs, int rhs) -> int { return lhs + rhs; };
        try {
            res = concore::conc_reduce(
                    integral_iterator(0), integral_iterator(num_iter), 0, op, reduction, hints);
            RC_FAIL("Exception was not properly thrown");
        } catch (const std::runtime_error& ex) {
            RC_ASSERT(std::string(ex.what()) == std::string("some error"));
        } catch (...) {
            RC_FAIL("Exception does not match");
        }
        RC_ASSERT(res < not_expected);
    });
}
TEST_CASE("conc_reduce forwards the exceptions in the reduce operation", "[conc_reduce]") {
    PROPERTY([](concore::partition_hints hints) {
        constexpr int num_iter = 100;
        int res = 0;
        int not_expected = (num_iter - 1) * num_iter / 2;

        auto op = [](int id, int i) -> int { return id + i; };
        auto reduction = [](int lhs, int rhs) -> int {
            if (lhs != 0)
                throw std::runtime_error("some error");
            return lhs + rhs;
        };
        try {
            res = concore::conc_reduce(
                    integral_iterator(0), integral_iterator(num_iter), 0, op, reduction, hints);
            RC_FAIL("Exception was not properly thrown");
        } catch (const std::runtime_error& ex) {
            RC_ASSERT(std::string(ex.what()) == std::string("some error"));
        } catch (...) {
            RC_FAIL("Exception does not match");
        }
        RC_ASSERT(res < not_expected);
    });
}
