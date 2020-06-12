
#include <catch2/catch.hpp>
#include <concore/conc_scan.hpp>
#include "arb_partition_hints.hpp"
#include <concore/integral_iterator.hpp>

#include <forward_list>

using concore::integral_iterator;

TEST_CASE("conc_scan is equivalent to std::inclusive_scan (vector)", "[conc_scan]") {
    PROPERTY([](concore::partition_hints hints) {
        const auto v = *rc::gen::container<std::vector<int>>(rc::gen::inRange(0, 1000));
        std::vector<int> res1(v.size(), 0);
        std::vector<int> res2(v.size(), 0);

        // Run conc_scan
        auto op = [](int id, int i) -> int { return id + i; };
        concore::conc_scan(v.begin(), v.end(), res1.begin(), 0, op, hints);

        // Run std::inclusive_scan
        std::inclusive_scan(v.begin(), v.end(), res2.begin(), op);

        // check the result
        RC_ASSERT(res1 == res2);
    });
}

TEST_CASE("conc_scan is equivalent to std::inclusive_scan (forward_list)", "[conc_scan]") {
    PROPERTY([](concore::partition_hints hints) {
        const auto v = *rc::gen::container<std::forward_list<int>>(rc::gen::inRange(0, 1000));
        int sz = std::distance(v.begin(), v.end());
        std::vector<int> res1(sz, 0);
        std::vector<int> res2(sz, 0);

        // Run conc_scan
        auto op = [](int id, int i) -> int { return id + i; };
        concore::conc_scan(v.begin(), v.end(), res1.begin(), 0, op, hints);

        // Run std::inclusive_scan
        std::inclusive_scan(v.begin(), v.end(), res2.begin(), op);

        // check the result
        RC_ASSERT(res1 == res2);
    });
}

TEST_CASE("conc_scan forwards the exceptions", "[conc_scan]") {
    PROPERTY([](concore::partition_hints hints) {
        constexpr int num_iter = 100;
        int res = 0;

        std::vector<int> dest(num_iter, 0);

        auto op = [](int id, int i) -> int {
            if (i == 0)
                throw std::runtime_error("some error");
            return id + i;
        };
        try {
            res = concore::conc_scan(
                    integral_iterator(0), integral_iterator(num_iter), dest.begin(), 0, op, hints);
            RC_FAIL("Exception was not properly thrown");
        } catch (const std::runtime_error& ex) {
            RC_ASSERT(std::string(ex.what()) == std::string("some error"));
        } catch (...) {
            RC_FAIL("Exception does not match");
        }
        RC_ASSERT(res == 0);
    });
}

TEST_CASE("conc_scan on non-commutative operations (static)", "[conc_scan]") {
    int sz = ('Z' - 'A' + 1) * 2;
    std::vector<std::string> v;
    v.reserve(sz);
    for (char c = 'A'; c <= 'Z'; c++)
        v.push_back(std::string(1, c));
    for (char c = 'a'; c <= 'z'; c++)
        v.push_back(std::string(1, c));
    std::vector<std::string> res1(sz, std::string{});
    std::vector<std::string> res2(sz, std::string{});

    // Run conc_scan
    auto op = [](std::string lhs, std::string rhs) -> std::string { return lhs + rhs; };
    concore::conc_scan(v.begin(), v.end(), res1.begin(), std::string{}, op);

    // Run std::inclusive_scan
    std::inclusive_scan(v.begin(), v.end(), res2.begin(), op);

    // check the result
    CHECK(res1 == res2);
}

TEST_CASE("conc_scan on non-commutative operations", "[conc_scan]") {
    PROPERTY(([](concore::partition_hints hints, std::vector<std::string> v) {
        std::vector<std::string> res1(v.size(), std::string{});
        std::vector<std::string> res2(v.size(), std::string{});

        // Run conc_scan
        auto op = [](std::string lhs, std::string rhs) -> std::string { return lhs + rhs; };
        concore::conc_scan(v.begin(), v.end(), res1.begin(), std::string{}, op);

        // Run std::inclusive_scan
        std::inclusive_scan(v.begin(), v.end(), res2.begin(), op);

        // check the result
        RC_ASSERT(res1 == res2);
    }));
}
