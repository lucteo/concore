#pragma once

#include <iterator>
#include <type_traits>

namespace concore {

inline namespace v1 {

/**
 * @brief      Pseudo-Iterator that allows representing integer ranges
 *
 * @tparam     T     The type of the integer pointed by the iterator
 *
 * This can be used in conc_for constructs to iterate over a range of integers.
 *
 * This is not a proper iterator, as it cannot take the reference to the element.
 *
 * Besides the fact that it's not a proper iterator, this comes close to a random-access iterator.
 */
template <typename T = int>
class integral_iterator : public std::iterator<std::random_access_iterator_tag, T> {
    T value_{};

public:
    using difference_type = T;

    integral_iterator() = default;
    ~integral_iterator() = default;
    integral_iterator(const integral_iterator&) = default;
    integral_iterator& operator=(const integral_iterator&) = default;

    explicit integral_iterator(T val)
        : value_(val) {}

    T operator*() const { return value_; }

    integral_iterator& operator++() {
        ++value_;
        return *this;
    }
    integral_iterator operator++(int) { return integral_iterator(value_++); }
    integral_iterator& operator--() {
        --value_;
        return *this;
    }
    integral_iterator operator--(int) { return integral_iterator(value_--); }

    T operator-(integral_iterator rhs) const { return value_ - rhs.value_; }

    integral_iterator operator+(T val) const { return integral_iterator(value_ + val); }
    integral_iterator operator-(T val) const { return integral_iterator(value_ - val); }

    bool operator==(integral_iterator rhs) const { return value_ == rhs.value_; }
    bool operator!=(integral_iterator rhs) const { return !(*this == rhs); }
};

} // namespace v1
} // namespace concore
