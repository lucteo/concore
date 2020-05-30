
#include <catch2/catch.hpp>
#include <concore/conc_reduce.hpp>
#include <concore/integral_iterator.hpp>
#include <concore/profiling.hpp>
#include <concore/init.hpp>

#include <thread>
#include <atomic>
#include <chrono>
#include <forward_list>

using namespace std::chrono_literals;

using concore::integral_iterator;

namespace {
void test_execute_once(concore::partition_hints hints) {
    constexpr int num_iter = 100;
    std::atomic<int> counts[num_iter] = {};
    for (int i = 0; i < num_iter; i++)
        counts[i] = 0;

    // Do the iterations in parallel
    auto op = [=, &counts](int id, int i) -> int {
        CONCORE_PROFILING_SCOPE_N("iter");
        CONCORE_PROFILING_SET_TEXT_FMT(32, "%d", i);
        REQUIRE(i >= 0);
        REQUIRE(i < num_iter);
        counts[i]++;
        return id + i;
    };
    auto reduction = [](int lhs, int rhs) -> int { return lhs + rhs; };
    concore::conc_reduce(
            integral_iterator(0), integral_iterator(num_iter), 0, op, reduction, hints);

    // Check that each iteration was executed only once
    for (int i = 0; i < num_iter; i++)
        REQUIRE(counts[i].load() == 1);
}

void test_sum(concore::partition_hints hints) {
    constexpr int num_iter = 10;

    // Do the iterations in parallel
    auto op = [](int id, int i) -> int { return id + i; };
    auto reduction = [](int lhs, int rhs) -> int { return lhs + rhs; };
    auto res = concore::conc_reduce(
            integral_iterator(0), integral_iterator(num_iter), 0, op, reduction, hints);

    // check the result
    auto expected = num_iter * (num_iter - 1) / 2;
    REQUIRE(res == expected);
}

void test_can_cancel(concore::partition_hints hints) {
    auto grp = concore::task_group::create();
    std::atomic<bool> first = true;

    // Do the iterations in parallel
    auto op = [=, &grp, &first](int id, int i) -> int {
        // Cancel the tasks in the group -- just do it in one task
        if (first.load()) {
            grp.cancel();
            first = false;
        }
        // While we are not canceled, spin around
        for (int i = 0; i < 1000; i++) {
            if (concore::task_group::is_current_task_cancelled())
                return 0;
            std::this_thread::sleep_for(1ms);
        }
        FAIL("Tasks are not properly canceled");
        return id + i;
    };
    auto reduction = [](int lhs, int rhs) -> int { return lhs + rhs; };
    concore::conc_reduce(integral_iterator(0), integral_iterator(1000), 0, op, reduction, grp, hints);
}

} // namespace

TEST_CASE("conc_reduce executes all its iterations exactly once", "[conc_reduce]") {
    SECTION("using the default auto_partition") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::auto_partition;
        test_execute_once(hints);
        hints.granularity_ = 2;
        test_execute_once(hints);
        hints.granularity_ = 10;
        test_execute_once(hints);
        hints.granularity_ = 100;
        test_execute_once(hints);
        hints.granularity_ = -1;
        test_execute_once(hints);
    }
    SECTION("using the iterative_partition method") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::iterative_partition;
        test_execute_once(hints);
        hints.granularity_ = 2;
        test_execute_once(hints);
        hints.granularity_ = 10;
        test_execute_once(hints);
        hints.granularity_ = 100;
        test_execute_once(hints);
        hints.granularity_ = -1;
        test_execute_once(hints);
    }
    SECTION("using the naive_partition method") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::naive_partition;
        test_execute_once(hints);
        hints.granularity_ = 2;
        test_execute_once(hints);
        hints.granularity_ = 10;
        test_execute_once(hints);
        hints.granularity_ = 100;
        test_execute_once(hints);
        hints.granularity_ = -1;
        test_execute_once(hints);
    }
    SECTION("using the upfront_partition method") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::upfront_partition;
        test_execute_once(hints);
    }
}

TEST_CASE("conc_reduce can implement parallel sum", "[conc_reduce]") {
    SECTION("using the default auto_partition") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::auto_partition;
        test_sum(hints);
        hints.granularity_ = 2;
        test_sum(hints);
        hints.granularity_ = 10;
        test_sum(hints);
        hints.granularity_ = 100;
        test_sum(hints);
        hints.granularity_ = -1;
        test_sum(hints);
    }
    SECTION("using the iterative_partition method") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::iterative_partition;
        test_sum(hints);
        hints.granularity_ = 2;
        test_sum(hints);
        hints.granularity_ = 10;
        test_sum(hints);
        hints.granularity_ = 100;
        test_sum(hints);
        hints.granularity_ = -1;
        test_sum(hints);
    }
    SECTION("using the naive_partition method") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::naive_partition;
        test_sum(hints);
        hints.granularity_ = 2;
        test_sum(hints);
        hints.granularity_ = 10;
        test_sum(hints);
        hints.granularity_ = 100;
        test_sum(hints);
        hints.granularity_ = -1;
        test_sum(hints);
    }
    SECTION("using the upfront_partition method") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::upfront_partition;
        test_sum(hints);
    }
}

TEST_CASE("conc_reduce can be canceled", "[conc_reduce]") {
    SECTION("using the default auto_partition") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::auto_partition;
        test_can_cancel(hints);
        hints.granularity_ = 2;
        test_can_cancel(hints);
        hints.granularity_ = 10;
        test_can_cancel(hints);
        hints.granularity_ = 100;
        test_can_cancel(hints);
        hints.granularity_ = -1;
        test_can_cancel(hints);
    }
    SECTION("using the iterative_partition method") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::iterative_partition;
        test_can_cancel(hints);
        hints.granularity_ = 2;
        test_can_cancel(hints);
        hints.granularity_ = 10;
        test_can_cancel(hints);
        hints.granularity_ = 100;
        test_can_cancel(hints);
        hints.granularity_ = -1;
        test_can_cancel(hints);
    }
    SECTION("using the naive_partition method") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::naive_partition;
        test_can_cancel(hints);
        hints.granularity_ = 2;
        test_can_cancel(hints);
        hints.granularity_ = 10;
        test_can_cancel(hints);
        hints.granularity_ = 100;
        test_can_cancel(hints);
        hints.granularity_ = -1;
        test_can_cancel(hints);
    }
    SECTION("using the upfront_partition method") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::upfront_partition;
        test_can_cancel(hints);
    }
}

TEST_CASE("conc_reduce can work with plain forward iterators", "[conc_reduce]") {
    constexpr int num_iter = 100;

    // Prepare the input data
    std::forward_list<int> elems;
    for (int i = num_iter - 1; i >= 0; i--) {
        elems.push_front(i);
    }

    // Do the iterations in parallel
    auto op = [](int id, int i) -> int { return id + i; };
    auto reduction = [](int lhs, int rhs) -> int { return lhs + rhs; };
    auto res = concore::conc_reduce(elems.begin(), elems.end(), 0, op, reduction);

    auto expected = num_iter * (num_iter - 1) / 2;
    REQUIRE(res == expected);
}
