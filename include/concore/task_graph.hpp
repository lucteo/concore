#pragma once

#include "task.hpp"
#include "executor_type.hpp"
#include "except_fun_type.hpp"
#include "spawn.hpp"
#include "profiling.hpp"
#include "detail/utils.hpp"
#include "detail/enqueue_next.hpp"

#include <memory>
#include <atomic>
#include <vector>
#include <initializer_list>

namespace concore {

inline namespace v1 {
class chained_task;
}

namespace detail {

//! The data for a chained_task
struct chained_task_impl : public std::enable_shared_from_this<chained_task_impl> {
    task task_fun_;
    std::atomic<int32_t> pred_count_{0};
    std::vector<chained_task> next_tasks_;
    executor_t executor_;
    except_fun_t except_fun_;

    chained_task_impl(task t, executor_t executor)
        : task_fun_(std::move(t))
        , executor_(executor) {
        if (!executor)
            executor_ = concore::spawn_executor{};
    }
};

} // namespace detail

inline namespace v1 {

class chained_task;

/**
 * @brief      A type of tasks that can be chained with other such tasks to create graphs of tasks.
 *
 * This is a wrapper on top of a @ref task, and cannot be directly interchanged with a @ref task.
 * This can directly enqueue the encapsulated @ref task, and also, one can create a @ref task on top
 * of this one (as this defines the call operator, and it's also a functor).
 *
 * One can create multiple @ref chained_task objects, then call @ref add_dependency() or@ref
 * add_dependencies() to create dependencies between such task objects. Thus, one can create graphs
 * of tasks from @ref chained_task objects.
 *
 * The built graph must be acyclic. Cyclic graphs can lead to execution stalls.
 *
 * After building the graph, the user should manually start the execution of the graph by enqueueing
 * a @ref chained_task that has no predecessors. After completion, this will try to enqueue
 * follow-up tasks, and so on, until all the graph is completely executed.
 *
 * A chained task will be executed only after all the predecessors have been executed. If a task has
 * three predecessors it will be executed only when the last predecessor completes. Looking from the
 * opposite direction, at the end of the task, the successors are checked; the number of active
 * predecessors is decremented, and, if one drops to zero, that successor will be enqueued.
 *
 * The @ref chained_task can be configured with an executor this will be used when enqueueing
 * successor tasks.
 *
 * If a task throws an exception, the handler in the associated @ref task_group is called (if set)
 * and the execution of the graph will continue. Similarly, if a task from the graph is canceled,
 * the execution of the graph will continue as if the task wasn't supposed to do anything.
 *
 * @see task, add_dependency(), add_dependencies(), task_group
 */
class chained_task {
public:
    /**
     * Default constructor. Constructs an invalid @ref chained_task.
     * Such a task cannot be placed in a graph of tasks.
     */
    chained_task() = default;

    /**
     * @brief      Constructor
     *
     * @param      t         The task to be executed
     * @param      executor  The executor to be used for the successor tasks (optional)
     *
     * This will initialize a valid @ref chained_task. After this constructor, @ref add_dependency()
     * and @ref add_dependencies() can be called to add predecessors and successors of this task.
     *
     * If this tasks tries to start executing successor tasks it will use the given executor.
     *
     * If no executor is given, the @ref spawn_executor will be used.
     *
     * @see add_dependency(), add_dependencies(), task
     */
    explicit chained_task(task t, executor_t executor = {})
        : impl_(std::make_shared<detail::chained_task_impl>(std::move(t), executor)) {}

    /**
     * @brief      The call operator.
     *
     * This will be called when executing the @ref chained_task. It will execute the task received
     * on constructor and then will check if it needs to start executing successors -- it will try
     * to start executing the successors that don't have any other active predecessors.
     *
     * This will use the executor given at construction to start successor tasks.
     */
    void operator()() {
        CONCORE_PROFILING_SCOPE_N("chained_task.()");
        assert(impl_->pred_count_.load() == 0);
        // Execute the current task
        detail::execute_task(impl_->task_fun_);

        // Try to execute the next tasks
        for (auto& n : impl_->next_tasks_) {
            if (n.impl_->pred_count_-- == 1) {
                chained_task next(std::move(n)); // don't keep the ref here anymore
                detail::enqueue_next(impl_->executor_, task(next), impl_->except_fun_);
            }
        }
        impl_->next_tasks_.clear();
    }

    /**
     * @brief      Sets the exception handler for enqueueing tasks
     *
     * @param      except_fun  The function to be called whenever an exception occurs.
     *
     * The exception handler set here will be called whenever an exception is thrown while
     * enqueueing a follow-up task. It will not be called whenever the task itself throws an
     * exception; that will be handled by the exception handler set in the group of the task.
     *
     * @see task_group::set_exception_handler
     */
    void set_exception_handler(except_fun_t except_fun) {
        impl_->except_fun_ = std::move(except_fun);
    }

    /**
     * @brief      Bool conversion operator.
     *
     * Indicates if this is a valid chained task.
     */
    explicit operator bool() const noexcept { return static_cast<bool>(impl_); }

    /**
     * @brief      Clear all the dependencies that go from this task
     *
     * This is useful for constructing and destroying task graphs manually.
     */
    void clear_next() {
        for (auto& n : impl_->next_tasks_)
            n.impl_->pred_count_--;
        impl_->next_tasks_.clear();
    }

private:
    std::shared_ptr<detail::chained_task_impl> impl_;

    friend void add_dependency(chained_task, chained_task);
    friend void add_dependencies(chained_task, std::initializer_list<chained_task>);
    friend void add_dependencies(std::initializer_list<chained_task>, chained_task);
};

/**
 * @brief      Add a dependency between two tasks.
 *
 * @param      prev  The task dependent on
 * @param      next  The task that depends on `prev`
 *
 * This creates a dependency between the given tasks. It means that `next` will only be executed
 * only after `prev` is completed.
 *
 * @see chained_task, add_dependencies()
 */
inline void add_dependency(chained_task prev, chained_task next) {
    next.impl_->pred_count_++;
    prev.impl_->next_tasks_.push_back(next);
}

/**
 * @brief      Add a dependency from a task to a list of tasks
 *
 * @param      prev   The task dependent on
 * @param      nexts  A set of tasks that all depend on `prev`
 *
 * This creates dependencies between `prev` and all the tasks in `nexts`. It's like calling @ref
 * add_dependency() multiple times.
 *
 * All the tasks in the `nexts` lists will not be started until `prev` is completed.
 *
 * @see chained_task, add_dependency()
 */
inline void add_dependencies(chained_task prev, std::initializer_list<chained_task> nexts) {
    for (const auto& n : nexts)
        n.impl_->pred_count_++;
    prev.impl_->next_tasks_.insert(prev.impl_->next_tasks_.end(), nexts.begin(), nexts.end());
}
/**
 * @brief      Add a dependency from list of tasks to a tasks
 *
 * @param      prevs  The list of tasks that `next` is dependent on
 * @param      next   The task that depends on all the `prevs` tasks
 *
 * This creates dependencies between all the tasks from `prevs` to the `next` task. It's like
 * calling @ref add_dependenc() multiple times.
 *
 * The `next` tasks will not start until all the tasks from the `prevs` list are complete.
 *
 * @see chained_task, add_dependency()
 */
inline void add_dependencies(std::initializer_list<chained_task> prevs, chained_task next) {
    next.impl_->pred_count_ += static_cast<int32_t>(prevs.size());
    for (const auto& p : prevs)
        p.impl_->next_tasks_.push_back(next);
}

} // namespace v1
} // namespace concore