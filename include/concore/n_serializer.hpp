#pragma once

#include "task.hpp"
#include "executor_type.hpp"
#include "data/concurrent_queue.hpp"
#include "detail/utils.hpp"

#include <memory>

namespace concore {

namespace detail {

//! The implementation details of an n_serializer
struct n_serializer_impl : public std::enable_shared_from_this<n_serializer_impl> {
    //! The base executor used to actually execute the tasks, once we've serialized them
    executor_t base_executor_;
    //! The function called to handle exceptions
    std::function<void(std::exception_ptr)> except_fun_;
    //! The number of tasks that can be executed in parallel
    int max_par_;
    //! The queue of tasks that wait to be executed
    concurrent_queue<task, queue_type::multi_prod_multi_cons> waiting_tasks_;
    //! The number of tasks that are in the queue, active or not
    std::atomic<uint32_t> combined_count_{0};

    union count_bits {
        uint32_t int_value;
        struct {
            unsigned num_active : 16;
            unsigned num_tasks : 16;
        } fields;
    };

    n_serializer_impl(
            executor_t base_executor, int N, std::function<void(std::exception_ptr)> except_fun)
        : base_executor_(base_executor)
        , max_par_(N)
        , except_fun_(except_fun) {}

    void enqueue(task&& t) {
        // Add the task to the queue
        waiting_tasks_.push(std::move(t));

        // Increase the number of tasks we keep track of.
        // Check if we can increase the "active" tasks; if so, enqueue a new task in the base
        // executor
        count_bits old, desired;
        old.int_value = combined_count_.load();
        do {
            desired.int_value = old.int_value;
            desired.fields.num_tasks++;
            if (desired.fields.num_active < max_par_)
                desired.fields.num_active++;
        } while (!combined_count_.compare_exchange_weak(old.int_value, desired.int_value));

        if (desired.fields.num_active != old.fields.num_active)
            enqueue_next();
    }

    //! Execute one task, and enqueues for execution the next task.
    //! This is called by the base executor
    //!
    //! Note: we only execute one task, even if we have multiple tasks. We do this to ensure that
    //! the tasks are relatively small sized.
    void execute_one() {
        detail_shared::pop_and_execute(waiting_tasks_, except_fun_);

        // Decrement the number of tasks
        // Check if we need to enqueue a continuation
        count_bits old, desired;
        old.int_value = combined_count_.load();
        do {
            desired.int_value = old.int_value;
            desired.fields.num_tasks--;
            desired.fields.num_active =
                    std::min(desired.fields.num_active, desired.fields.num_tasks);
        } while (!combined_count_.compare_exchange_weak(old.int_value, desired.int_value));

        if (desired.fields.num_active == old.fields.num_active)
            enqueue_next();
    }

    //! Enqueue the next task to be executed in the base executor.
    void enqueue_next() {
        // We always wrap our tasks into `execute_one`. This way, we can handle continuations.
        base_executor_([p_this = shared_from_this()]() { p_this->execute_one(); });
    }
};
} // namespace detail

inline namespace v1 {

//! Executor type that allows maximum N tasks to be executed at a given time.
//!
//! This will be constructed on top of an existing executor, and can be considered an executor
//! itself.
//!
//! Guarantees:
//! - no more than N task are executed at once
//! - if N==1, behaves like the `serializer` class
class n_serializer : public std::enable_shared_from_this<n_serializer> {
public:
    n_serializer(executor_t base_executor, int N,
            std::function<void(std::exception_ptr)> except_fun = {})
        : impl_(std::make_shared<detail::n_serializer_impl>(base_executor, N, except_fun)) {}

    //! The call operator that makes this an executor.
    //! Enqueues the given task in the base serializer, making sure that we don't execute too many
    //! tasks at the given time in parallel.
    void operator()(task t) { impl_->enqueue(std::move(t)); }

private:
    //! The implementation object of this n_serializer.
    //! We need this to be shared pointer for lifetime issue, but also to be able to copy the
    //! serializer easily.
    std::shared_ptr<detail::n_serializer_impl> impl_;
};

} // namespace v1
} // namespace concore