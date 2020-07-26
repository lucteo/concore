#pragma once

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

} // namespace detail
} // namespace concore
