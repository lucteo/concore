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

    virtual void execute(task t) const = 0;
    virtual executor_base* clone() const = 0;
    virtual bool is_same(const executor_base& other) const = 0;
    virtual const std::type_info& target_type() const noexcept = 0;
    virtual void* target() noexcept = 0;
    virtual const void* target() const noexcept = 0;
};

template <CONCORE_CONCEPT_OR_TYPENAME(executor) Executor>
struct executor_wrapper : executor_base {
    explicit executor_wrapper(Executor e)
        : exec_(std::forward<Executor>(e)) {}

    void execute(task t) const override { concore::execute(exec_, std::move(t)); }
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
 * and wraps its execution.
 *
 * If the any executor is is not initialized with a valid executor, then calling @ref execute() on
 * it is undefined behavior.
 *
 * @ref executor, inline_executor, global_executor
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
     * This will construct the object by wrapping the given executor. The given executor must be
     * valid.
     */
    template <typename Executor>
    any_executor(Executor e)
        : wrapper_(new detail::executor_wrapper<Executor>(std::forward<Executor>(e))) {}

    any_executor& operator=(const any_executor& other) noexcept {
        any_executor(other).swap(*this);
        return *this;
    }
    any_executor& operator=(any_executor&& other) noexcept {
        any_executor(std::move(other)).swap(*this);
        return *this;
    }
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
     * This will implement assignment from another executor. The given executor must be valid.
     */
    template <typename Executor>
    any_executor& operator=(Executor e) {
        any_executor(std::forward<Executor>(e)).swap(*this);
        return *this;
    }

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
     * This implements the `execute()` CPO for this executor object, making it conform to the
     * executor concept. It forwards the call to the underlying executor.
     *
     * @see executor
     */
    template <typename F>
    void execute(F&& f) const {
        assert(wrapper_);
        wrapper_->execute(task{std::forward<F>(f)});
    }
    //! @overload
    void execute(task t) const {
        assert(wrapper_);
        wrapper_->execute(std::move(t));
    }

    // TODO (now): Remove this
    void operator()(task t) const { execute(std::move(t)); }

    //! Checks if this executor is wrapping another executor
    explicit operator bool() const noexcept { return wrapper_ != nullptr; }

    //! Returns the type_info for the wrapped executor type
    const std::type_info& target_type() const noexcept {
        return wrapper_ ? wrapper_->target_type() : typeid(std::nullptr_t);
    }

    //! Helper method to get the underlying executor, if its type is specified
    template <CONCORE_CONCEPT_OR_TYPENAME(executor) Executor>
    Executor* target() noexcept {
        return wrapper_ && wrapper_->target_type() == typeid(Executor)
                       // NOLINTNEXTLINE
                       ? reinterpret_cast<Executor*>(wrapper_->target())
                       : nullptr;
    }
    //! @overload
    template <CONCORE_CONCEPT_OR_TYPENAME(executor) Executor>
    const Executor* target() const noexcept {
        // NOLINTNEXTLINE
        return wrapper_ ? reinterpret_cast<const Executor*>(wrapper_->target()) : nullptr;
    }

    //! Comparison operator
    friend inline bool operator==(any_executor l, any_executor r) {
        if (!l && !r)
            return true;
        else if (l && r && l.target_type() == r.target_type())
            return l.wrapper_->is_same(*r.wrapper_);
        else
            return false;
    }
    //! @overload
    friend inline bool operator==(any_executor l, std::nullptr_t) { return l.wrapper_ == nullptr; }
    //! @overload
    friend inline bool operator==(std::nullptr_t, any_executor r) { return r.wrapper_ == nullptr; }
    //! @overload
    friend inline bool operator!=(any_executor l, any_executor r) { return !(l == r); }
    //! @overload
    friend inline bool operator!=(any_executor l, std::nullptr_t r) { return !(l == r); }
    //! @overload
    friend inline bool operator!=(std::nullptr_t l, any_executor r) { return !(l == r); }

private:
    //! The wrapped executor object
    detail::executor_base* wrapper_{nullptr};
};
} // namespace v1

} // namespace concore