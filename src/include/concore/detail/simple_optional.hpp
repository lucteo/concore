#pragma once

#include <type_traits>
#include <new>

namespace concore {

namespace detail {

template <typename T>
struct simple_optional {
    using storage_t = typename std::aligned_storage<sizeof(T), alignof(T)>::type;
    storage_t value_{};
    bool isSet_{false};

    simple_optional() = default;

    template <typename... Args>
    simple_optional(Args&&... vals) {
        construct(std::forward<Args>(vals)...);
        isSet_ = true;
    }
    simple_optional(simple_optional&& rhs) noexcept {
        if (rhs.isSet_) {
            construct(std::forward<T>(rhs.value()));
            isSet_ = true;
        }
    }
    simple_optional(const simple_optional& rhs) {
        if (rhs.isSet_) {
            construct(rhs.value());
            isSet_ = true;
        }
    }
    ~simple_optional() { destruct(); }

    simple_optional& operator=(simple_optional&& rhs) noexcept {
        if (isSet_ == rhs.isSet_) {
            if (isSet_)
                value() = std::forward<T>(rhs.value());
        } else {
            if (isSet_)
                destruct();
            else
                construct(std::forward<T>(rhs.value()));
            isSet_ = rhs.isSet_;
        }
        return *this;
    }
    simple_optional& operator=(const simple_optional& rhs) {
        if (isSet_ == rhs.isSet_) {
            if (isSet_)
                value() = rhs.value();
        } else {
            if (isSet_)
                destruct();
            else
                construct(rhs.value());
            isSet_ = rhs.isSet_;
        }
        return *this;
    }

    template <typename... Args>
    void emplace(Args&&... vals) {
        if (isSet_)
            destruct();
        new (&value_) T(std::forward<Args>(vals)...);
        isSet_ = true;
    }

    void reset() {
        if (isSet_)
            destruct();
        isSet_ = false;
    }

    T& value() { return reinterpret_cast<T&>(value_); }                   // NOLINT
    const T& value() const { return reinterpret_cast<const T&>(value_); } // NOLINT

private:
    template <typename... Args>
    void construct(Args&&... vals) {
        new (&value_) T(std::forward<Args>(vals)...);
    }
    void destruct() { value().~T(); }
};

} // namespace detail
} // namespace concore
