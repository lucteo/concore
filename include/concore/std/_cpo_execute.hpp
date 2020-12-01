#pragma once

#include <concore/detail/cxx_features.hpp>

namespace concore {

namespace std_execution {

namespace detail {
namespace cpo_execute {

#if CONCORE_CXX_HAS_CONCEPTS

template <typename Tag, typename E, typename F>
concept has_tag_invoke_execute = requires(const E& e, F&& f) {
    tag_invoke(Tag{}, (E &&) e, (F &&) f);
};

template <typename Tag, typename E, typename F>
concept has_inner_execute = !has_tag_invoke_execute<Tag, E, F> && requires(const E& e, F&& f) {
    e.execute((F &&) f);
};

template <typename Tag, typename E, typename F>
concept has_outer_execute = !has_tag_invoke_execute<Tag, E, F> && !has_inner_execute<Tag, E, F> &&
                            requires(const E& e, F&& f) {
    execute((E &&) e, (F &&) f);
};

template <typename Tag, typename E, typename F>
concept can_tag_invoke = !has_inner_execute<Tag, E, F> && requires(const E& e, F&& f) {
    tag_invoke(Tag{}, (E &&) e, (F &&) f);
};

inline const struct execute_t final {
    template <typename Executor, typename Fn>
    requires has_tag_invoke_execute<execute_t, Executor, Fn> //
            void operator()(Executor&& e, Fn&& f) const
            noexcept(noexcept(tag_invoke(*this, (Executor &&) e, (Fn &&) f))) {
        tag_invoke(execute_t{}, (Executor &&) e, (Fn &&) f);
    }
    template <typename Executor, typename Fn>
    requires has_inner_execute<execute_t, Executor, Fn> //
            void operator()(Executor&& e, Fn&& f) const
            noexcept(noexcept(((Executor &&) e).execute((Fn &&) f))) {
        ((Executor &&) e).execute((Fn &&) f);
    }
    template <typename Executor, typename Fn>
    requires has_outer_execute<execute_t, Executor, Fn> //
            void operator()(Executor&& e, Fn&& f) const
            noexcept(noexcept(execute((Executor &&) e, (Fn &&) f))) {
        execute((Executor &&) e, (Fn &&) f);
    }
    // TODO: sender
} execute{};

#else

template <typename T>
T valOfType();

template <typename...>
struct tag_tag_invoke {};

template <typename...>
struct tag_inner_execute {};

template <typename...>
struct tag_outer_execute {};

template <typename T, typename E, typename F,
        typename = decltype(tag_invoke(T{}, valOfType<E>(), valOfType<F>()))>
char tester(tag_tag_invoke<T, E, F>*);
template <typename E, typename F, typename = decltype(valOfType<E>().execute(valOfType<F>()))>
char tester(tag_inner_execute<E, F>*);
template <typename E, typename F, typename = decltype(execute(valOfType<E>(), valOfType<F>()))>
char tester(tag_outer_execute<E, F>*);
double tester(...);

template <typename T, typename E, typename F>
inline constexpr bool has_tag_invoke_execute = sizeof(tester(static_cast<tag_tag_invoke<T, E, F>*>(
                                                       nullptr))) == 1;

template <typename T, typename E, typename F>
inline constexpr bool has_inner_execute =
        !has_tag_invoke_execute<T, E, F> &&
        sizeof(tester(static_cast<tag_inner_execute<E, F>*>(nullptr))) == 1;

template <typename T, typename E, typename F>
inline constexpr bool has_outer_execute =
        !has_tag_invoke_execute<T, E, F> && !has_inner_execute<T, E, F> &&
        sizeof(tester(static_cast<tag_outer_execute<E, F>*>(nullptr))) == 1;

inline const struct execute_t final {
    template <typename Executor, typename Fn,
            std::enable_if_t<has_tag_invoke_execute<execute_t, Executor, Fn>, int> = 0>
    void operator()(Executor&& e, Fn&& f) const
            noexcept(noexcept(tag_invoke(*this, (Executor &&) e, (Fn &&) f))) {
        tag_invoke(execute_t{}, (Executor &&) e, (Fn &&) f);
    }

    template <typename Executor, typename Fn,
            std::enable_if_t<has_inner_execute<execute_t, Executor, Fn>, int> = 0>
    void operator()(Executor&& e, Fn&& f) const
            noexcept(noexcept(((Executor &&) e).execute((Fn &&) f))) {
        ((Executor &&) e).execute((Fn &&) f);
    }

    template <typename Executor, typename Fn,
            std::enable_if_t<has_outer_execute<execute_t, Executor, Fn>, int> = 0>
    void operator()(Executor&& e, Fn&& f) const
            noexcept(noexcept(execute((Executor &&) e, (Fn &&) f))) {
        execute((Executor &&) e, (Fn &&) f);
    }
    // TODO: sender
} execute{};
#endif

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
