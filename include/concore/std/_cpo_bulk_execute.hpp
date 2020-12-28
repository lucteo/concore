#pragma once

#include <concore/detail/concept_macros.hpp>

namespace concore {
namespace std_execution {

namespace detail {
namespace cpo_bulk_execute {

CONCORE_DEF_REQUIRES(meets_tag_invoke, //
        CONCORE_LIST(typename Tag, typename E, typename F, typename N),
        CONCORE_LIST(Tag, E, F, N), //
        (requires(
                const E& e, F&& f, N n) { tag_invoke(Tag{}, (E &&) e, (F &&) f, n); }), // concepts
        tag_invoke(
                Tag{}, CONCORE_DECLVAL(E), CONCORE_DECLVAL(F), CONCORE_DECLVAL(N)) // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_inner_fun,                                            //
        CONCORE_LIST(typename E, typename F, typename N), CONCORE_LIST(E, F, N), //
        (requires(const E& e, F&& f, N n) { e.bulk_execute((F &&) f, n); }),     // concepts
        CONCORE_DECLVAL(E).bulk_execute(CONCORE_DECLVAL(F), CONCORE_DECLVAL(N))  // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_outer_fun,                                                //
        CONCORE_LIST(typename E, typename F, typename N), CONCORE_LIST(E, F, N),     //
        (requires(const E& e, F&& f, N n) { bulk_execute((E &&) e, (F &&) f, n); }), // concepts
        bulk_execute(CONCORE_DECLVAL(E), CONCORE_DECLVAL(F), CONCORE_DECLVAL(N))     // pre-concepts
);

template <typename Tag, typename E, typename F, typename N>
CONCORE_CONCEPT_OR_BOOL(has_tag_invoke) = meets_tag_invoke<Tag, E, F, N>;

template <typename Tag, typename E, typename F, typename N>
CONCORE_CONCEPT_OR_BOOL(has_inner_fun) = !has_tag_invoke<Tag, E, F, N> && meets_inner_fun<E, F, N>;

template <typename Tag, typename E, typename F, typename N>
CONCORE_CONCEPT_OR_BOOL(has_outer_fun) = !has_tag_invoke<Tag, E, F, N> &&
                                         !has_inner_fun<Tag, E, F, N> && meets_outer_fun<E, F, N>;

inline const struct bulk_execute_t final {
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename E, typename F, typename N),
            (has_tag_invoke<bulk_execute_t, E, F, N>))
    void operator()(E&& e, F&& f, N n) const                               //
            noexcept(noexcept(tag_invoke(*this, (E &&) e, (F &&) f, n))) { //
        tag_invoke(bulk_execute_t{}, (E &&) e, (F &&) f, n);
    }
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename E, typename F, typename N),
            (has_inner_fun<bulk_execute_t, E, F, N>))
    void operator()(E&& e, F&& f, N n) const                           //
            noexcept(noexcept(((E &&) e).bulk_execute((F &&) f, n))) { //
        ((E &&) e).bulk_execute((F &&) f, n);
    }
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename E, typename F, typename N),
            (has_outer_fun<bulk_execute_t, E, F, N>))
    void operator()(E&& e, F&& f, N n) const                          //
            noexcept(noexcept(bulk_execute((E &&) e, (F &&) f, n))) { //
        bulk_execute((E &&) e, (F &&) f, n);
    }
} bulk_execute{};

} // namespace cpo_bulk_execute
} // namespace detail

inline namespace v1 {
/**
 * @brief   Function-like object that can be used to bulk_execute work on executors
 *
 * @tparam  E   The type of executor we want to use; must model `executor_of<F>`
 * @tparam  F   The type of functor we want toe bulk_execute through our executor
 *
 * @param   e   The executor object we are using for our execution
 * @param   f   The functor to be invoked
 *
 * This will tell the executor object to invoke the given functor, according to the rules defined in
 * the executor.
 *
 * `executor_of<E, F, N>` must be true.
 */
using detail::cpo_bulk_execute::bulk_execute;

/**
 * @brief   Customization-point-object tag for bulk_execute
 *
 * To add support for bulk_execute to a type T, one can define:
 *      template <typename F, typename N>
 *      void tag_invoke(bulk_execute_t, T, F, N);
 */
using detail::cpo_bulk_execute::bulk_execute_t;
} // namespace v1

} // namespace std_execution
} // namespace concore
