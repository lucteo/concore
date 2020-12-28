#pragma once

#include <concore/detail/concept_macros.hpp>

namespace concore {
namespace std_execution {

namespace detail {
namespace cpo_execute {

CONCORE_DEF_REQUIRES(meets_tag_invoke,                                               //
        CONCORE_LIST(typename Tag, typename E, typename F), CONCORE_LIST(Tag, E, F), //
        (requires(const E& e, F&& f) { tag_invoke(Tag{}, (E &&) e, (F &&) f); }),    // concepts
        tag_invoke(Tag{}, CONCORE_DECLVAL(E), CONCORE_DECLVAL(F))                    // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_inner_fun,                             //
        CONCORE_LIST(typename E, typename F), CONCORE_LIST(E, F), //
        (requires(const E& e, F&& f) { e.execute((F &&) f); }),   // concepts
        CONCORE_DECLVAL(E).execute(CONCORE_DECLVAL(F))            // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_outer_fun,                                   //
        CONCORE_LIST(typename E, typename F), CONCORE_LIST(E, F),       //
        (requires(const E& e, F&& f) { execute((E &&) e, (F &&) f); }), // concepts
        execute(CONCORE_DECLVAL(E), CONCORE_DECLVAL(F))                 // pre-concepts
);

template <typename Tag, typename E, typename F>
CONCORE_CONCEPT_OR_BOOL(has_tag_invoke) = meets_tag_invoke<Tag, E, F>;

template <typename Tag, typename E, typename F>
CONCORE_CONCEPT_OR_BOOL(has_inner_fun) = !has_tag_invoke<Tag, E, F> && meets_inner_fun<E, F>;

template <typename Tag, typename E, typename F>
CONCORE_CONCEPT_OR_BOOL(has_outer_fun) = !has_tag_invoke<Tag, E, F> &&
                                         !has_inner_fun<Tag, E, F> && meets_outer_fun<E, F>;

inline const struct execute_t final {
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename E, typename F), (has_tag_invoke<execute_t, E, F>))
    void operator()(E&& e, F&& f) const                                 //
            noexcept(noexcept(tag_invoke(*this, (E &&) e, (F &&) f))) { //
        tag_invoke(execute_t{}, (E &&) e, (F &&) f);
    }
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename E, typename F), (has_inner_fun<execute_t, E, F>))
    void operator()(E&& e, F&& f) const                        //
            noexcept(noexcept(((E &&) e).execute((F &&) f))) { //
        ((E &&) e).execute((F &&) f);
    }
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename E, typename F), (has_outer_fun<execute_t, E, F>))
    void operator()(E&& e, F&& f) const                       //
            noexcept(noexcept(execute((E &&) e, (F &&) f))) { //
        execute((E &&) e, (F &&) f);
    }
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
 * @brief   Type to use for customization point for execute
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
