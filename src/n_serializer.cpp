#include "concore/n_serializer.hpp"
#include "concore/global_executor.hpp"
#include "concore/spawn.hpp"
#include "concore/data/concurrent_queue.hpp"
#include "concore/detail/utils.hpp"

#include <atomic>
#include <cassert>

namespace concore {

inline namespace v1 {

struct n_serializer::impl : std::enable_shared_from_this<impl> {
    //! The base executor used to actually execute the tasks, when we enqueue them
    executor_t base_executor_;
    //! The executor to be used when
    executor_t cont_executor_;
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

    impl(int N, executor_t base_executor, executor_t cont_executor)
        : base_executor_(base_executor)
        , cont_executor_(cont_executor)
        , max_par_(N) {
        if (!base_executor)
            base_executor_ = global_executor;
        if (!cont_executor)
            cont_executor_ = base_executor ? base_executor : spawn_continuation_executor;
    }

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
            enqueue_next(base_executor_);
    }

    //! Execute one task, and enqueues for execution the next task.
    //! This is called by the base executor
    //!
    //! Note: we only execute one task, even if we have multiple tasks. We do this to ensure that
    //! the tasks are relatively small sized.
    void execute_one() {
        detail::pop_and_execute(waiting_tasks_);

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
            enqueue_next(cont_executor_);
    }

    //! Enqueue the next task to be executed in the given executor.
    void enqueue_next(const executor_t& executor) {
        // We always wrap our tasks into `execute_one`. This way, we can handle continuations.
        executor([p_this = shared_from_this()]() { p_this->execute_one(); });
    }
};

n_serializer::n_serializer(int N, executor_t base_executor, executor_t cont_executor)
    : impl_(std::make_shared<impl>(N, base_executor, cont_executor)) {}

void n_serializer::operator()(task t) { impl_->enqueue(std::move(t)); }

} // namespace v1
} // namespace concore