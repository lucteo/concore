/**
 * @file    task_graph.hpp
 * @brief   Utilities for creating graphs of tasks
 *
 * @see     @ref concore::v1::chained_task "chained_task", add_dependency(), add_dependencies()
 */
#pragma once

#include "task.hpp"
#include "any_executor.hpp"
#include "except_fun_type.hpp"

#include <memory>
#include <atomic>
#include <vector>
#include <initializer_list>

namespace concore {

inline namespace v1 {
class chained_task;
}

namespace detail {
struct chained_task_impl;
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
     * @details
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
    explicit chained_task(task t, any_executor executor = {});
    //! @overload
    template <typename F>
    explicit chained_task(F f, any_executor executor = {})
        : chained_task(task{std::forward<F>(f)}, executor) {}

    /**
     * @brief      The call operator.
     *
     * @details
     *
     * This will be called when executing the @ref chained_task. It will execute the task received
     * on constructor and then will check if it needs to start executing successors -- it will try
     * to start executing the successors that don't have any other active predecessors.
     *
     * This will use the executor given at construction to start successor tasks.
     *
     * The exceptions raised during the execution of the task, will be reported to the continuation
     * of the task executed (and to the task group of the task). In any case, the successor tasks
     * will be executed.
     */
    void operator()() noexcept;

    /**
     * @brief      Bool conversion operator.
     *
     * Indicates if this is a valid chained task.
     */
    explicit operator bool() const noexcept;

    /**
     * @brief      Clear all the dependencies that go from this task
     *
     * This is useful for constructing and destroying task graphs manually.
     */
    void clear_next() noexcept;

private:
    //! Implementation data for a chained task
    std::shared_ptr<detail::chained_task_impl> impl_;
    //! The task that should be executed; we actually execute copies of these objects
    std::shared_ptr<task> to_execute_;

    friend struct detail::chained_task_impl;
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
 * @details
 *
 * This creates a dependency between the given tasks. It means that `next` will only be executed
 * only after `prev` is completed.
 *
 * @see chained_task, add_dependencies()
 */
void add_dependency(chained_task prev, chained_task next);

/**
 * @brief      Add a dependency from a task to a list of tasks
 *
 * @param      prev   The task dependent on
 * @param      nexts  A set of tasks that all depend on `prev`
 *
 * @details
 *
 * This creates dependencies between `prev` and all the tasks in `nexts`. It's like calling @ref
 * add_dependency() multiple times.
 *
 * All the tasks in the `nexts` lists will not be started until `prev` is completed.
 *
 * @see chained_task, add_dependency()
 */
void add_dependencies(chained_task prev, std::initializer_list<chained_task> nexts);
/**
 * @brief      Add a dependency from list of tasks to a tasks
 *
 * @param      prevs  The list of tasks that `next` is dependent on
 * @param      next   The task that depends on all the `prevs` tasks
 *
 * @details
 *
 * This creates dependencies between all the tasks from `prevs` to the `next` task. It's like
 * calling @ref add_dependency() multiple times.
 *
 * The `next` tasks will not start until all the tasks from the `prevs` list are complete.
 *
 * @see chained_task, add_dependency()
 */
void add_dependencies(std::initializer_list<chained_task> prevs, chained_task next);

} // namespace v1
} // namespace concore