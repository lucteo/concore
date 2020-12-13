#pragma once

#include <concore/detail/concept_macros.hpp>

namespace concore {

namespace std_execution {

namespace detail {
namespace cpo_execute {

using concore::detail::valOfType;

// clang-format off
CONCORE_CONCEPT_REQUIRES_CASE3(meets_tag_invoke,
    (requires(const T2& e, T3&& f) {
        tag_invoke(T1{}, (T2 &&) e, (T3 &&) f);
    }));
CONCORE_CONCEPT_REQUIRES_CASE2(meets_inner_execute,
    (requires(const T1& e, T2&& f) {
        e.execute((T2 &&) f);
    }));
CONCORE_CONCEPT_REQUIRES_CASE2(meets_outer_execute,
    (requires(const T1& e, T2&& f) {
        execute((T1 &&) e, (T2 &&) f);
    }));
// clang-format on

CONCORE_NONCONCEPT_REQUIRES_DEFAULTCASE();
CONCORE_NONCONCEPT_REQUIRES_CASE3(
        meets_tag_invoke, tag_invoke(T1{}, valOfType<T2>(), valOfType<T3>()));
CONCORE_NONCONCEPT_REQUIRES_CASE2(meets_inner_execute, valOfType<T1>().execute(valOfType<T2>()));
CONCORE_NONCONCEPT_REQUIRES_CASE2(meets_outer_execute, execute(valOfType<T1>(), valOfType<T2>()));

// clang-format off
template <typename Tag, typename E, typename F>
CONCORE_CONCEPT_OR_BOOL(has_tag_invoke_execute)
    = meets_tag_invoke<Tag, E, F>;

template <typename Tag, typename E, typename F>
CONCORE_CONCEPT_OR_BOOL(has_inner_execute)
    = !has_tag_invoke_execute<Tag, E, F>
    && meets_inner_execute<E, F>;

template <typename Tag, typename E, typename F>
CONCORE_CONCEPT_OR_BOOL(has_outer_execute)
    = !has_tag_invoke_execute<Tag, E, F>
    && !has_inner_execute<Tag, E, F>
    && meets_outer_execute<E, F>;
// clang-format on

inline const struct execute_t final {
    template <typename Executor, typename Fn,
            CONCORE_NONCONCEPT_EXTRA_TEMPLATE_COND(
                    (has_tag_invoke_execute<execute_t, Executor, Fn>))>
    CONCORE_CONCEPT_REQUIRES_COND((has_tag_invoke_execute<execute_t, Executor, Fn>))
    void operator()(Executor&& e, Fn&& f) const
            noexcept(noexcept(tag_invoke(*this, (Executor &&) e, (Fn &&) f))) {
        tag_invoke(execute_t{}, (Executor &&) e, (Fn &&) f);
    }
    template <typename Executor, typename Fn,
            CONCORE_NONCONCEPT_EXTRA_TEMPLATE_COND((has_inner_execute<execute_t, Executor, Fn>))>
    CONCORE_CONCEPT_REQUIRES_COND((has_inner_execute<execute_t, Executor, Fn>))
    void operator()(Executor&& e, Fn&& f) const
            noexcept(noexcept(((Executor &&) e).execute((Fn &&) f))) {
        ((Executor &&) e).execute((Fn &&) f);
    }
    template <typename Executor, typename Fn,
            CONCORE_NONCONCEPT_EXTRA_TEMPLATE_COND((has_outer_execute<execute_t, Executor, Fn>))>
    CONCORE_CONCEPT_REQUIRES_COND((has_outer_execute<execute_t, Executor, Fn>))
    void operator()(Executor&& e, Fn&& f) const
            noexcept(noexcept(execute((Executor &&) e, (Fn &&) f))) {
        execute((Executor &&) e, (Fn &&) f);
    }
    // TODO: sender
} execute{};

} // namespace cpo_execute
} // namespace detail

inline namespace v1 {
/**
 * @brief   Function-like object that can be used to execute work on executors
 *
 * @tparam  E   The type of executor we want to use; must model `executor_of<F>`
 * @tparam  F   The type of functor we want toe execute through our executor
 *
 * @param   e   The executor object we are using for our execution
 * @param   f   The functor to be invoked
 *
 * This will tell the executor object to invoke the given functor, according to the rules defined in
 * the executor.
 *
 * `executor_of<E, F>` must be true.
 */
using detail::cpo_execute::execute;

/**
 * @brief   Type to use for customization point
 *
 * This can be used for types that do not directly model the executor concepts. One can define a
 * `tag_invoke` customization point to make the type be an executor.
 *
 * For any given type `Ex`, and a functor type `Fn`, defining
 *      void tag_invoke(execute_t, Ex, Fn) {...}
 *
 * will make the `executor_of<Ex, Fn>` be true. that is, one can later call:
 *      execute(ex, f);
 * , where `ex` is an object of type `Ex`, and `fn` is an object of type `Fn`.
 *
 * @see     execute
 */
using detail::cpo_execute::execute_t;
} // namespace v1

} // namespace std_execution
} // namespace concore
