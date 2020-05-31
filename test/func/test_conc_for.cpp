
#include <catch2/catch.hpp>
#include <concore/conc_for.hpp>
#include <concore/integral_iterator.hpp>
#include <concore/profiling.hpp>
#include <concore/init.hpp>

#include <thread>
#include <atomic>
#include <chrono>
#include <forward_list>

using namespace std::chrono_literals;

using concore::integral_iterator;

void test_execute_once(concore::partition_hints hints) {
    constexpr int num_iter = 100;
    std::atomic<int> counts[num_iter] = {};
    for (int i = 0; i < num_iter; i++)
        counts[i] = 0;

    // Do the iterations in parallel
    concore::conc_for(integral_iterator(0), integral_iterator(num_iter),
            [=, &counts](int i) {
                CONCORE_PROFILING_SCOPE_N("iter");
                CONCORE_PROFILING_SET_TEXT_FMT(32, "%d", i);
                REQUIRE(i >= 0);
                REQUIRE(i < num_iter);
                counts[i]++;
            },
            hints);

    // Check that each iteration was executed only once
    for (int i = 0; i < num_iter; i++)
        REQUIRE(counts[i].load() == 1);
}

TEST_CASE("conc_for executes all its iterations exactly once", "[conc_for]") {
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

void test_can_cancel(concore::partition_hints hints) {
    auto grp = concore::task_group::create();
    std::atomic<bool> first = true;

    // Do the iterations in parallel
    concore::conc_for(integral_iterator(0), integral_iterator(1000),
            [&grp, &first](int i) {
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
                FAIL("Tasks are not properly canceled");
            },
            grp, hints);
}

TEST_CASE("conc_for can be canceled", "[conc_for]") {
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

TEST_CASE("conc_for is a blocking call", "[conc_for]") {
    constexpr int num_iter = 100;
    std::atomic<int> count = 0;

    // Do the iterations in parallel
    concore::conc_for(integral_iterator(0), integral_iterator(num_iter), [&count](int i) {
        std::this_thread::sleep_for(1ms);
        count++;
    });

    // If we would exit earlier, count would not reach num_iter
    REQUIRE(count.load() == num_iter);
}

TEST_CASE("conc_for can work with plain forward iterators", "[conc_for]") {
    constexpr int num_iter = 100;
    std::atomic<int> counts[num_iter];

    // Prepare the input data
    std::forward_list<int> elems;
    for (int i = num_iter - 1; i >= 0; i--) {
        elems.push_front(i);
        counts[i] = 0;
    }

    // Do the iterations in parallel
    concore::conc_for(elems.begin(), elems.end(), [&counts](int i) { counts[i]++; });

    // Incremented each count once
    for (int i = 0; i < num_iter; i++)
        REQUIRE(counts[i].load() == 1);
}

namespace {
void test_exceptions(concore::partition_hints hints) {
    constexpr int num_iter = 100;
    std::atomic<int> count = 0;

    // Do the iterations in parallel
    auto iter_work = [&count](int i) {
        if (i == 0)
            throw std::runtime_error("some error");
        std::this_thread::sleep_for(1ms);
        if (!concore::task_group::is_current_task_cancelled())
            count++;
    };
    try {
        concore::conc_for(integral_iterator(0), integral_iterator(num_iter), iter_work, hints);
        FAIL("Exception was not properly thrown");
    }
    catch(const std::runtime_error& ex) {
        REQUIRE(std::string(ex.what()) == std::string("some error"));
    }
    catch(...) {
        FAIL("Exception does not match");
    }
    REQUIRE(count.load() < num_iter);
}
}

TEST_CASE("conc_for forwards the exceptions", "[conc_for]") {
    SECTION("using the default auto_partition") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::auto_partition;
        test_exceptions(hints);
    }
    SECTION("using the iterative_partition method") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::iterative_partition;
        test_exceptions(hints);
    }
    SECTION("using the naive_partition method") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::naive_partition;
        test_exceptions(hints);
    }
    SECTION("using the upfront_partition method") {
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::upfront_partition;
        test_exceptions(hints);
    }
}
