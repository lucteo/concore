#pragma once

#include "concore/partition_hints.hpp"
#include "concore/detail/task_system.hpp"

#include <iterator>

namespace concore {
namespace detail {

//! Structure that indicates that we don't have an iterator in the C++ way
struct no_iterator_tag {};

//! Helper to determine the iterator category of a given type; can be an iterator category or
//! no_iterator_tag
template <typename T>
struct iterator_type {
private:
    template <typename U>
    static typename U::iterator_category test(typename U::iterator_category* p = nullptr);
    template <typename U>
    static no_iterator_tag test(...);

public:
    using iterator_category = decltype(test<T>(nullptr));
};

//! Safe-dereference an iterator or an integral value.
//! If the given type is an iterator, it will dereference it the standard way; if, not, this will
//! return the given type; therefore we can "safely dereference" integral numbers.
//!
//! Usage: safe_dereference(it, nullptr);
//! The second parameter is mandatory.
template <typename T>
inline typename T::reference safe_dereference(T it, typename T::iterator_category* p = nullptr) {
    return *it;
}
template <typename T>
inline T safe_dereference(T it, ...) {
    return it;
}

//! Returns the next iterator of the given iterator
template <typename It>
It it_next(It it) {
    it++;
    return it;
}

inline int compute_granularity(int n, partition_hints hints) {
    int granularity = std::max(1, hints.granularity_);
    int max_tasks_per_worker = hints.tasks_per_worker_ > 0 ? hints.tasks_per_worker_ : 20;
    int min_granularity =
            n / (detail::get_task_system().num_worker_threads() * max_tasks_per_worker);
    return std::max(granularity, min_granularity);
}

} // namespace detail
} // namespace concore