#pragma once

#include <catch2/catch.hpp>
#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_done.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>

namespace empty_recv {

using concore::set_done_t;
using concore::set_error_t;
using concore::set_value_t;

struct recv0 {
    friend void tag_invoke(set_value_t, recv0&&) {}
    friend void tag_invoke(set_done_t, recv0&&) noexcept {}
    friend void tag_invoke(set_error_t, recv0&&, std::exception_ptr) noexcept {}
};
struct recv_int {
    friend void tag_invoke(set_value_t, recv_int&&, int) {}
    friend void tag_invoke(set_done_t, recv_int&&) noexcept {}
    friend void tag_invoke(set_error_t, recv_int&&, std::exception_ptr) noexcept {}
};

struct recv0_ec {
    friend void tag_invoke(set_value_t, recv0_ec&&) {}
    friend void tag_invoke(set_done_t, recv0_ec&&) noexcept {}
    friend void tag_invoke(set_error_t, recv0_ec&&, std::error_code) noexcept {}
    friend void tag_invoke(set_error_t, recv0_ec&&, std::exception_ptr) noexcept {}
};
struct recv_int_ec {
    friend void tag_invoke(set_value_t, recv_int_ec&&, int) {}
    friend void tag_invoke(set_done_t, recv_int_ec&&) noexcept {}
    friend void tag_invoke(set_error_t, recv_int_ec&&, std::error_code) noexcept {}
    friend void tag_invoke(set_error_t, recv_int_ec&&, std::exception_ptr) noexcept {}
};

} // namespace empty_recv

class expect_void_receiver {
    bool called_{false};

public:
    expect_void_receiver() = default;
    ~expect_void_receiver() { CHECK(called_); }

    expect_void_receiver(expect_void_receiver&& other)
        : called_(other.called_) {
        other.called_ = true;
    }
    expect_void_receiver& operator=(expect_void_receiver&& other) {
        called_ = other.called_;
        other.called_ = true;
        return *this;
    }

    friend void tag_invoke(concore::set_value_t, expect_void_receiver&& self) noexcept {
        self.called_ = true;
    }
    template <typename... Ts>
    friend void tag_invoke(concore::set_value_t, expect_void_receiver&&, Ts...) noexcept {
        FAIL_CHECK("set_value called on expect_void_receiver with some non-void value");
    }
    friend void tag_invoke(concore::set_done_t, expect_void_receiver&&) noexcept {
        FAIL_CHECK("set_done called on expect_void_receiver");
    }
    friend void tag_invoke(
            concore::set_error_t, expect_void_receiver&&, std::exception_ptr) noexcept {
        FAIL_CHECK("set_error called on expect_void_receiver");
    }
};

template <typename T>
class expect_value_receiver {
    bool called_{false};
    T value_;

public:
    explicit expect_value_receiver(T val)
        : value_(std::forward<T>(val)) {}
    ~expect_value_receiver() { CHECK(called_); }

    expect_value_receiver(expect_value_receiver&& other)
        : called_(other.called_) {
        other.called_ = true;
    }
    expect_value_receiver& operator=(expect_value_receiver&& other) {
        called_ = other.called_;
        other.called_ = true;
        return *this;
    }

    template <typename... Ts>
    friend void tag_invoke(
            concore::set_value_t, expect_value_receiver&& self, const T& val) noexcept {
        CHECK(val == self.value_);
        self.called_ = true;
    }
    template <typename... Ts>
    friend void tag_invoke(concore::set_value_t, expect_value_receiver&&, Ts...) noexcept {
        FAIL_CHECK("set_value called with wrong value types on expect_value_receiver");
    }
    friend void tag_invoke(concore::set_done_t, expect_value_receiver&& self) noexcept {
        FAIL_CHECK("set_done called on expect_value_receiver");
    }
    friend void tag_invoke(
            concore::set_error_t, expect_value_receiver&&, std::exception_ptr) noexcept {
        FAIL_CHECK("set_error called on expect_value_receiver");
    }
};

class expect_done_receiver {
    bool called_{false};

public:
    expect_done_receiver() = default;
    ~expect_done_receiver() { CHECK(called_); }

    expect_done_receiver(expect_done_receiver&& other)
        : called_(other.called_) {
        other.called_ = true;
    }
    expect_done_receiver& operator=(expect_done_receiver&& other) {
        called_ = other.called_;
        other.called_ = true;
        return *this;
    }

    template <typename... Ts>
    friend void tag_invoke(concore::set_value_t, expect_done_receiver&&, Ts...) noexcept {
        FAIL_CHECK("set_value called on expect_done_receiver");
    }
    friend void tag_invoke(concore::set_done_t, expect_done_receiver&& self) noexcept {
        self.called_ = true;
    }
    friend void tag_invoke(
            concore::set_error_t, expect_done_receiver&&, std::exception_ptr) noexcept {
        FAIL_CHECK("set_error called on expect_done_receiver");
    }
};

class expect_error_receiver {
    bool called_{false};

public:
    expect_error_receiver() = default;
    ~expect_error_receiver() { CHECK(called_); }

    expect_error_receiver(expect_error_receiver&& other)
        : called_(other.called_) {
        other.called_ = true;
    }
    expect_error_receiver& operator=(expect_error_receiver&& other) {
        called_ = other.called_;
        other.called_ = true;
        return *this;
    }

    template <typename... Ts>
    friend void tag_invoke(concore::set_value_t, expect_error_receiver&&, Ts...) noexcept {
        FAIL_CHECK("set_value called on expect_error_receiver");
    }
    friend void tag_invoke(concore::set_done_t, expect_error_receiver&& self) noexcept {
        FAIL_CHECK("set_done called on expect_error_receiver");
    }
    friend void tag_invoke(
            concore::set_error_t, expect_error_receiver&& self, std::exception_ptr) noexcept {
        self.called_ = true;
    }
    template <typename E>
    friend void tag_invoke(concore::set_error_t, expect_error_receiver&& self, E) noexcept {
        self.called_ = true;
    }
};

struct logging_receiver {
    int* state_;
    bool should_throw_{false};
    friend void tag_invoke(concore::set_value_t, logging_receiver&& self) {
        if (self.should_throw_)
            throw std::logic_error("test");
        *self.state_ = 0;
    }
    friend void tag_invoke(concore::set_done_t, logging_receiver&& self) noexcept {
        *self.state_ = 1;
    }
    friend void tag_invoke(
            concore::set_error_t, logging_receiver&& self, std::exception_ptr) noexcept {
        *self.state_ = 2;
    }
};
