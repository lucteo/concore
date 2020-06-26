
#include <catch2/catch.hpp>
#include "rapidcheck_utils.hpp"
#include <concore/conc_sort.hpp>

#include <forward_list>

using namespace std::chrono_literals;

TEST_CASE("conc_sort properly sorts a given range", "[conc_sort]") {
    std::vector<int> v;
    static constexpr int num_elem = 1000;
    v.reserve(num_elem);
    for (int i=num_elem-1; i>=0; i--)
        v.push_back(i/10);

    concore::conc_sort(v.begin(), v.end());
    CHECK(std::is_sorted(v.begin(), v.end()));
}

TEST_CASE("conc_sort properly sorts ranges", "[conc_sort]") {
    PROPERTY([](std::vector<int> v) {
        concore::conc_sort(v.begin(), v.end());
        RC_ASSERT(std::is_sorted(v.begin(), v.end()));
    });
}
TEST_CASE("conc_sort keeps the same elements", "[conc_sort]") {
    PROPERTY([](std::vector<int> v) {
        auto v_copy = v;
        concore::conc_sort(v.begin(), v.end());

        // Same count
        RC_ASSERT(v.size() == v_copy.size());

        // Sorting the elements should yield the same elements
        std::sort(v_copy.begin(), v_copy.end());
        RC_ASSERT(v == v_copy);
    });
}
