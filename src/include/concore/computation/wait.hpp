#pragma once

#include <concore/computation/computation.hpp>
#include <concore/spawn.hpp>
#include <concore/task_cancelled.hpp>

namespace concore {
namespace computation {
namespace detail {

struct wait_recv_base {
    task_group grp_;
    std::exception_ptr& ex_;

    wait_recv_base(task_group grp, std::exception_ptr& ex)
        : grp_(std::move(grp))
        , ex_(ex) {}

    void set_value() { concore::detail::task_group_access::on_task_destroyed(grp_); }
    void set_done() noexcept {
        try {
            throw task_cancelled{};
        } catch (...) {
            ex_ = std::current_exception();
        }
        concore::detail::task_group_access::on_task_destroyed(grp_);
    }
    void set_error(std::exception_ptr ex) noexcept {
        ex_ = ex;
        concore::detail::task_group_access::on_task_destroyed(grp_);
    }
};

template <typename T>
struct wait_recv : wait_recv_base {
    T* val_;

    wait_recv(task_group grp, std::exception_ptr& ex, T* val)
        : wait_recv_base(std::move(grp), ex)
        , val_(val) {}

    void set_value(T val) {
        *val_ = (T &&) val;
        wait_recv_base::set_value();
    }
};

template <>
struct wait_recv<void> : wait_recv_base {
    wait_recv(task_group grp, std::exception_ptr& ex, void* = nullptr)
        : wait_recv_base(std::move(grp), ex) {}
};

template <typename Comp, typename ValT>
inline void do_wait(Comp comp, ValT* res) {
    // Create a group and keep it busy
    auto grp = concore::task_group::create();
    concore::detail::task_group_access::on_task_created(grp);

    // Start the computation and retain the results
    std::exception_ptr ex;
    run_with((Comp &&) comp, wait_recv{grp, ex, res});

    // Wait until the computation is completely done
    concore::wait(grp);

    // If there is an error or cancellation, rethrow here
    if (ex)
        std::rethrow_exception(ex);
}

} // namespace detail

inline namespace v1 {

/**
 * @brief   Runs a computation and wait for its result
 * @tparam  Comp    The type of the computation to run
 * @param   comp    The computation that need to be run
 * @return  The result of the computation
 * @details
 *
 * This will run a computation and wait for the result. If the computation yields a non-void value,
 * this function will return the value yielded by the computation. If the computation results with
 * an error the error is thrown in the context of this function. If the computation is cancelled
 * this function will throw task_cancelled.
 *
 * The waiting employed here is a busy-wait. While the computation is not yet done and there are
 * other tasks that can be executed, these will begin to execute. This will try to keep the
 * throughput high, but it may have latency issues.
 *
 * Post-conditions:
 * - if the computation yields a non-void value, the result of this will be the value type of the
 * computation
 * - if the computation has a value type of void, this will return void
 * - if the computation yields a non-void value, this function will return it
 * - if the computation ends with an error, this function will throw that error
 * - if the computation is canceled, this function will throw task_cancelled
 * - the computation is run & consumed before the function exists
 *
 * @see     just_value(), transform(), computation
 */
template <typename Comp>
inline typename Comp::value_type wait(Comp comp) {
    using res_t = typename Comp::value_type;
    if constexpr (std::is_void_v<res_t>) {
        detail::do_wait<Comp, res_t>((Comp &&) comp, nullptr);
    } else {
        res_t res;
        detail::do_wait<Comp, res_t>((Comp &&) comp, &res);
        return res;
    }
}

} // namespace v1
} // namespace computation
} // namespace concore