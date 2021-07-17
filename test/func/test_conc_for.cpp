
#include <catch2/catch.hpp>
#include "arb_partition_hints.hpp"
#include <concore/conc_for.hpp>
#include <concore/integral_iterator.hpp>
#include <concore/profiling.hpp>
#include <concore/init.hpp>

#include <thread>
#include <atomic>
#include <chrono>
#include <forward_list>
#include <array>

using namespace std::chrono_literals;

using concore::integral_iterator;

TEST_CASE("conc_for executes all its iterations exactly once", "[conc_for]") {
    PROPERTY(([](concore::partition_hints hints) {
        constexpr int num_iter = 100;
        std::array<std::atomic<int>, num_iter> counts{};
        for (int i = 0; i < num_iter; i++)
            counts[i] = 0;

        // Do the iterations in parallel
        auto iter_body = [&](int i) {
            CONCORE_PROFILING_SCOPE_N("iter");
            CONCORE_PROFILING_SET_TEXT_FMT(32, "%d", i);
            RC_ASSERT(i >= 0);
            RC_ASSERT(i < num_iter);
            counts[i]++;
        };
        concore::conc_for(integral_iterator(0), integral_iterator(num_iter), iter_body, hints);

        // Check that each iteration was executed only once
        for (int i = 0; i < num_iter; i++)
            RC_ASSERT(counts[i].load() == 1);
    }));
}

TEST_CASE("conc_for can be canceled", "[conc_for]") {
    PROPERTY([](concore::partition_hints hints) {
        auto grp = concore::task_group::create();
        std::atomic<bool> first = true;

        // Do the iterations in parallel
        auto iter_body = [&](int i) {
            // Cancel the tasks in the group -- just do it in one task
            if (first.load()) {
                grp.cancel();
                first = false;
            }
            // While we are not canceled, spin around
            for (int i = 0; i < 1000; i++) {
                if (concore::task_group::is_current_task_cancelled())
                    return;
                std::this_thread::sleep_for(1ms);
            }
            RC_FAIL("Tasks are not properly canceled");
        };
        concore::conc_for(integral_iterator(0), integral_iterator(1000), iter_body, grp, hints);
    });
}

TEST_CASE("conc_for is a blocking call", "[conc_for]") {
    PROPERTY([](concore::partition_hints hints) {
        constexpr int num_iter = 20;
        std::atomic<int> count = 0;

        // Do the iterations in parallel
        auto iter_body = [&count](int i) {
            std::this_thread::sleep_for(1us);
            count++;
        };
        concore::conc_for(integral_iterator(0), integral_iterator(num_iter), iter_body, hints);

        // If we would exit earlier, count would not reach num_iter
        RC_ASSERT(count.load() == num_iter);
    });
}

TEST_CASE("conc_for can work with plain forward iterators", "[conc_for]") {
    PROPERTY(([](concore::partition_hints hints) {
        constexpr int num_iter = 100;
        std::array<std::atomic<int>, num_iter> counts{};

        // Prepare the input data
        std::forward_list<int> elems;
        for (int i = num_iter - 1; i >= 0; i--) {
            elems.push_front(i);
            counts[i] = 0;
        }

        // Do the iterations in parallel
        concore::conc_for(
                elems.begin(), elems.end(), [&counts](int i) { counts[i]++; }, hints);

        // Incremented each count once
        for (int i = 0; i < num_iter; i++)
            RC_ASSERT(counts[i].load() == 1);
    }));
}

// TEST_CASE("conc_for forwards the exceptions", "[conc_for]") {
//     PROPERTY([](concore::partition_hints hints) {
//         constexpr int num_iter = 100;
//         std::atomic<int> count = 0;
//
//         // Do the iterations in parallel
//         auto iter_work = [&count](int i) {
//             if (i == 0)
//                 throw std::runtime_error("some error");
//             if (!concore::task_group::is_current_task_cancelled())
//                 count++;
//         };
//         try {
//             concore::conc_for(integral_iterator(0), integral_iterator(num_iter), iter_work,
//             hints); RC_FAIL("Exception was not properly thrown");
//         } catch (const std::runtime_error& ex) {
//             RC_ASSERT(std::string(ex.what()) == std::string("some error"));
//         } catch (...) {
//             RC_FAIL("Exception does not match");
//         }
//         RC_ASSERT(count.load() < num_iter);
//     });
// }

TEST_CASE("conc_for can be used to implement transform", "[conc_for]") {
    PROPERTY([](concore::partition_hints hints, std::vector<int> v) {
        auto f = [](int x) { return x / 3 * 2; };
        std::vector<int> my_res(v.size());

        auto n = int(v.size());
        auto iter_work = [&](int idx) { my_res[idx] = f(v[idx]); };
        concore::conc_for(integral_iterator(0), integral_iterator(n), iter_work, hints);

        std::vector<int> expected(v.size());
        std::transform(v.begin(), v.end(), expected.begin(), f);

        RC_ASSERT(my_res == expected);
    });
}
