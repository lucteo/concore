/**
 * @file    any_executor.hpp
 * @brief   Defines the @ref concore::v1::any_executor "any_executor" class
 */
#pragma once

#include "task.hpp"
#include "_concepts/_concepts_executor.hpp"
#include "_cpo/_cpo_execute.hpp"

#include <functional>
#include <typeinfo>
#include <utility>
#include <cstddef>
#include <cassert>

namespace concore {

namespace detail {
struct executor_base {
    executor_base() = default;
    virtual ~executor_base() = default;

    executor_base(const executor_base&) = delete;
    executor_base(executor_base&&) = delete;
    executor_base& operator=(const executor_base&) = delete;
    executor_base& operator=(executor_base&&) = delete;

    virtual void execute(task_function f) const = 0;
    virtual void execute(task t) const noexcept = 0;
    virtual executor_base* clone() const = 0;
    virtual bool is_same(const executor_base& other) const = 0;
    virtual const std::type_info& target_type() const noexcept = 0;
    virtual void* target() noexcept = 0;
    virtual const void* target() const noexcept = 0;
};

template <typename Executor>
struct executor_wrapper : executor_base {
    static_assert(task_executor<Executor>, "Type needs to match task_executor concept");

    explicit executor_wrapper(Executor e)
        : exec_(std::forward<Executor>(e)) {}

    void execute(task_function f) const override { concore::execute(exec_, std::move(f)); }
    void execute(task t) const noexcept override { concore::execute(exec_, std::move(t)); }
    executor_wrapper* clone() const override { return new executor_wrapper(exec_); }
    bool is_same(const executor_base& other) const override {
        return exec_ == static_cast<const executor_wrapper&>(other).exec_;
    }
    const std::type_info& target_type() const noexcept override { return typeid(Executor); }
    void* target() noexcept override { return &exec_; }
    const void* target() const noexcept override { return &exec_; }

    //! The actual executor instance
    Executor exec_;
};

} // namespace detail

inline namespace v1 {

/**
 * @brief A polymorphic executor wrapper
 *
 * This provides a type erasure on an executor type. It can hold any type of executor object in it
 * and wraps its execution. The type of the wrapped object must model the @ref executor concept.
 *
 * If the any executor is is not initialized with a valid executor, then calling @ref execute() on
 * it is undefined behavior.
 *
 * @see executor, inline_executor, global_executor
 */
class any_executor {
public:
    //! Constructs an invalid executor.
    any_executor() noexcept = default;
    //! Constructs an invalid executor.
    explicit any_executor(std::nullptr_t) noexcept {}
    //! Copy constructor
    any_executor(const any_executor& other) noexcept
        : wrapper_(other ? other.wrapper_->clone() : nullptr) {}
    //! Move constructor
    any_executor(any_executor&& other) noexcept
        : wrapper_(other.wrapper_) {
        other.wrapper_ = nullptr;
    }

    /**
     * @brief Constructor from a valid executor
     *
     * @param e The executor to be wrapped by this object
     *
     * @details
     *
     * This will construct the object by wrapping the given executor. The given executor must be
     * valid and must model the @ref executor concept.
     *
     * @see executor
     */
    template <typename Executor>
    any_executor(Executor e)
        : wrapper_(new detail::executor_wrapper<Executor>(std::forward<Executor>(e))) {}

    //! Copy assignment
    any_executor& operator=(const any_executor& other) noexcept {
        any_executor(other).swap(*this);
        return *this;
    }
    //! Move assignment
    any_executor& operator=(any_executor&& other) noexcept {
        any_executor(std::move(other)).swap(*this);
        return *this;
    }
    //! Copy assignment from nullptr_t
    any_executor& operator=(std::nullptr_t) noexcept {
        any_executor{}.swap(*this);
        return *this;
    }

    /**
     * @brief Assignment operator from another executor.
     *
     * @param e The executor to be wrapped in this object
     * @return Reference to this object
     *
     * @details
     *
     * This will implement assignment from another executor. The given executor must be valid and
     * must model the @ref executor concept.
     *
     * @see executor
     */
    template <typename Executor>
    any_executor& operator=(Executor e) {
        any_executor(std::forward<Executor>(e)).swap(*this);
        return *this;
    }

    //! Destructor
    ~any_executor() { delete wrapper_; }

    //! Swaps the content of this object with the content of the given object
    void swap(any_executor& other) noexcept {
        using std::swap;
        swap(wrapper_, other.wrapper_);
    }

    /**
     * @brief The main execution method of this executor
     *
     * @param f Functor to be executed in the wrapped executor
     *
     * @details
     *
     * This implements the `execute()` CPO for this executor object, making it conform to the
     * executor concept. It forwards the call to the underlying executor.
     *
     * @see executor
     */
    template <typename F>
    void execute(F&& f) const {
        assert(wrapper_);
        wrapper_->execute(std::forward<F>(f));
    }
    /**
     * @brief The main execution method of this executor
     *
     * @param t The task to be executed in the wrapped executor
     *
     * @details
     *
     * This implements the `execute()` CPO for this executor object, making it conform to the
     * executor concept. It forwards the call to the underlying executor.
     *
     * This guarantees that no exceptions will be thrown. If, some exceptions appear in the
     * enqueuing of the task, then these will be reported to the continuation function stored in the
     * task.
     *
     * @see executor
     */
    void execute(task t) const noexcept {
        assert(wrapper_);
        wrapper_->execute(std::move(t));
    }

    //! Checks if this executor is wrapping another executor
    explicit operator bool() const noexcept { return wrapper_ != nullptr; }

    //! Returns the type_info for the wrapped executor type
    const std::type_info& target_type() const noexcept {
        return wrapper_ ? wrapper_->target_type() : typeid(std::nullptr_t);
    }

    //! Helper method to get the underlying executor, if its type is specified
    template <typename Executor>
    Executor* target() noexcept {
        return wrapper_ && wrapper_->target_type() == typeid(Executor)
                       // NOLINTNEXTLINE
                       ? reinterpret_cast<Executor*>(wrapper_->target())
                       : nullptr;
    }
    //! @overload
    template <typename Executor>
    const Executor* target() const noexcept {
        // NOLINTNEXTLINE
        return wrapper_ ? reinterpret_cast<const Executor*>(wrapper_->target()) : nullptr;
    }

    //! Equality operator
    friend inline bool operator==(const any_executor& l, const any_executor& r) {
        if (!l && !r)
            return true;
        else if (l && r && l.target_type() == r.target_type())
            return l.wrapper_->is_same(*r.wrapper_);
        else
            return false;
    }
    //! @overload
    friend inline bool operator==(const any_executor& l, std::nullptr_t) {
        return l.wrapper_ == nullptr;
    }
    //! @overload
    friend inline bool operator==(std::nullptr_t, const any_executor& r) {
        return r.wrapper_ == nullptr;
    }
    //! Inequality operator
    friend inline bool operator!=(const any_executor& l, const any_executor& r) {
        return !(l == r);
    }
    //! @overload
    friend inline bool operator!=(const any_executor& l, std::nullptr_t r) { return !(l == r); }
    //! @overload
    friend inline bool operator!=(std::nullptr_t l, const any_executor& r) { return !(l == r); }

private:
    //! The wrapped executor object
    detail::executor_base* wrapper_{nullptr};
};
} // namespace v1

} // namespace concore