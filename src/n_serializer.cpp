#include "concore/n_serializer.hpp"
#include "concore/global_executor.hpp"
#include "concore/spawn.hpp"
#include "concore/detail/consumer_bounded_queue.hpp"
#include "concore/detail/utils.hpp"
#include "concore/detail/enqueue_next.hpp"

#include <atomic>
#include <cassert>

namespace concore {

inline namespace v1 {

struct n_serializer::impl : std::enable_shared_from_this<impl> {
    //! The base executor used to actually execute the tasks, when we enqueue them
    any_executor base_executor_;
    //! The executor to be used when
    any_executor cont_executor_;
    //! Handler to be called whenever we have an exception while enqueueing the next task
    except_fun_t except_fun_;
    //! The tasks that are enqueued into our object
    detail::consumer_bounded_queue<task> processing_items_;

    impl(int N, any_executor base_executor, any_executor cont_executor)
        : base_executor_(base_executor)
        , cont_executor_(cont_executor)
        , processing_items_(N) {
        if (!base_executor)
            base_executor_ = global_executor{};
        if (!cont_executor)
            cont_executor_ = base_executor ? base_executor : spawn_continuation_executor{};
    }

    void enqueue(task&& t) {
        if (processing_items_.push_and_try_acquire(std::move(t)))
            enqueue_next(base_executor_);
    }

    //! Execute one task, and enqueues for execution the next task.
    //! This is called by the base executor
    //!
    //! Note: we only execute one task, even if we have multiple tasks. We do this to ensure that
    //! the tasks are relatively small sized.
    void execute_one() {
        // Execute one task
        auto to_execute = processing_items_.extract_one();
        detail::execute_task(to_execute);

        // Can we start a new task?
        if (processing_items_.release_and_acquire())
            enqueue_next(cont_executor_);
    }

    //! Enqueue the next task to be executed in the given executor.
    void enqueue_next(any_executor& executor) {
        // We always wrap our tasks into `execute_one`. This way, we can handle continuations.
        auto f = [p_this = shared_from_this()]() { p_this->execute_one(); };
        detail::enqueue_next(executor, task{std::move(f)}, except_fun_);
    }
};

n_serializer::n_serializer(int N, any_executor base_executor, any_executor cont_executor)
    : impl_(std::make_shared<impl>(N, base_executor, cont_executor)) {}

void n_serializer::do_enqueue(task t) const { impl_->enqueue(std::move(t)); }

void n_serializer::set_exception_handler(except_fun_t except_fun) {
    impl_->except_fun_ = std::move(except_fun);
}

} // namespace v1
} // namespace concore