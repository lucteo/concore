#include "concore/serializer.hpp"
#include "concore/global_executor.hpp"
#include "concore/spawn.hpp"
#include "concore/data/concurrent_queue.hpp"
#include "concore/detail/utils.hpp"
#include "concore/detail/enqueue_next.hpp"

#include <atomic>
#include <cassert>

namespace concore {

inline namespace v1 {

//! The implementation details of a serializer
struct serializer::impl : std::enable_shared_from_this<impl> {
    //! The base executor used to actually execute the tasks, when we enqueue them
    any_executor base_executor_;
    //! The executor to be used when
    any_executor cont_executor_;
    //! Handler to be called whenever we have an exception while enqueueing the next task
    except_fun_t except_fun_;
    //! The queue of tasks that wait to be executed
    concurrent_queue<task, queue_type::multi_prod_single_cons> waiting_tasks_;
    //! The number of tasks that are in the queue
    std::atomic<int> count_{0};

    impl(any_executor base_executor, any_executor cont_executor)
        : base_executor_(base_executor)
        , cont_executor_(cont_executor) {
        if (!base_executor)
            base_executor_ = global_executor{};
        if (!cont_executor)
            cont_executor_ = base_executor ? base_executor : spawn_continuation_executor{};
    }

    //! Adds a new task to this serializer
    void enqueue(task&& t) {
        // Add the task to the queue
        waiting_tasks_.push(std::forward<task>(t));

        // If there were no other tasks, enqueue a task in the base executor
        if (count_++ == 0)
            enqueue_next(base_executor_);
    }

    //! Called by the base executor to execute one task.
    void execute_one() {
        detail::pop_and_execute(waiting_tasks_);

        // If there are still tasks in the queue, continue to enqueue the next task
        if (count_-- > 1)
            enqueue_next(cont_executor_);
    }

    //! Enqueue the next task to be executed in the given executor.
    void enqueue_next(any_executor& executor) {
        // We always wrap our tasks into `execute_one`. This way, we can handle continuations.
        task t = [p_this = shared_from_this()]() { p_this->execute_one(); };
        detail::enqueue_next(executor, std::move(t), except_fun_);
    }
};

serializer::serializer(any_executor base_executor, any_executor cont_executor)
    : impl_(std::make_shared<impl>(base_executor, cont_executor)) {}

void serializer::do_enqueue(task t) const { impl_->enqueue(std::move(t)); }

void serializer::set_exception_handler(except_fun_t except_fun) {
    impl_->except_fun_ = std::move(except_fun);
}

} // namespace v1
} // namespace concore