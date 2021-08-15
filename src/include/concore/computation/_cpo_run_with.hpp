#pragma once

#include <concore/detail/cxx_features.hpp>
#include <concore/detail/extra_type_traits.hpp>
#include <concore/detail/concept_macros.hpp>

#if CONCORE_CXX_HAS_CONCEPTS
#include <concepts>
#endif
#include <type_traits>

namespace concore {

namespace computation {

namespace detail {
namespace cpo_run_with {

CONCORE_DEF_REQUIRES(meets_tag_invoke,                                               //
        CONCORE_LIST(typename Tag, typename C, typename R), CONCORE_LIST(Tag, C, R), //
        (requires(C&& c, R&& r) { tag_invoke(Tag{}, (C &&) c, (R &&) r); }),         // concepts
        tag_invoke(Tag{}, CONCORE_DECLVAL(C), CONCORE_DECLVAL(R))                    // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_inner_fun,                             //
        CONCORE_LIST(typename C, typename R), CONCORE_LIST(C, R), //
        (requires(C&& c, R&& r) { c.run_with((R &&) r); }),       // concepts
        CONCORE_DECLVAL(C).run_with(CONCORE_DECLVAL(R))           // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_outer_fun,                               //
        CONCORE_LIST(typename C, typename R), CONCORE_LIST(C, R),   //
        (requires(C&& c, R&& r) { run_with((C &&) c, (R &&) r); }), // concepts
        run_with(CONCORE_DECLVAL(C), CONCORE_DECLVAL(R))            // pre-concepts
);

template <typename Tag, typename C, typename R>
CONCORE_CONCEPT_OR_BOOL(has_tag_invoke) = meets_tag_invoke<Tag, C, R>;

template <typename Tag, typename C, typename R>
CONCORE_CONCEPT_OR_BOOL(has_inner_fun) = !has_tag_invoke<Tag, C, R> && meets_inner_fun<C, R>;

template <typename Tag, typename C, typename R>
CONCORE_CONCEPT_OR_BOOL(has_outer_fun) = !has_tag_invoke<Tag, C, R> &&
                                         !has_inner_fun<Tag, C, R> && meets_outer_fun<C, R>;

inline const struct run_with_t final {
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename C, typename R), (has_tag_invoke<run_with_t, C, R>))
    void operator()(C&& c, R&& r) const                                 //
            noexcept(noexcept(tag_invoke(*this, (C &&) c, (R &&) r))) { //
        tag_invoke(run_with_t{}, (C &&) c, (R &&) r);
    }
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename C, typename R), (has_inner_fun<run_with_t, C, R>))
    void operator()(C&& c, R&& r) const                         //
            noexcept(noexcept(((C &&) c).run_with((R &&) r))) { //
        ((C &&) c).run_with((R &&) r);
    }
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename C, typename R), (has_outer_fun<run_with_t, C, R>))
    void operator()(C&& c, R&& r) const                        //
            noexcept(noexcept(run_with((C &&) c, (R &&) r))) { //
        run_with((C &&) c, (R &&) r);
    }
} run_with{};

template <typename C, typename R>
CONCORE_CONCEPT_OR_BOOL(has_run_with) =
        meets_tag_invoke<run_with_t, C, R> || meets_inner_fun<C, R> || meets_outer_fun<C, R>;

} // namespace cpo_run_with
} // namespace detail

inline namespace v1 {

/**
 * @brief CPO for void run_with(computation, receiver).
 *
 * @param computation   the computation that need to be run
 * @param receiver      the receiver that will get the completion signal from the computation
 *
 * @details
 *
 * This CPO essentially introduces a functor object with the following signature:
 * @code{.cpp}
 *      template <typename C, typename Recv>
 *      void run_with(C computation, Recv receiver);
 * @endcode
 *
 * By using this CPO, we allow the user to define the `run_with` functions for a computation C in
 * three ways:
 * 1) by creating a tag_invoke specialization
 * 2) by adding `run_with` as a method inside C
 * 3) by adding `run_with(C, R)` as a free function near C
 *
 * Example for creating a tag_invoke specialization:
 * @code{.cpp}
 *      template <typename C, typename Recv>
 *      void run_with(run_with_t, C computation, Recv receiver);
 * @endcode
 *
 * Example for using a method inside class C
 * @code{.cpp}
 *      struct C {
 *          ...
 *          template <typename Recv>
 *          void run_with(Recv receiver);
 *      };
 * @endcode
 *
 * Example for using a free function outside C
 * @code{.cpp}
 *      struct C;
 *
 *      template <typename Recv>
 *      void run_with(C computation, Recv receiver);
 * @endcode
 *
 * This is needed for the @ref computation concept. It is also used in the library to start
 * computations. The end-user typically doesn't have to call this.
 *
 * @see computation
 */
using detail::cpo_run_with::run_with;

/**
 * @brief Tag type to be used to specialize `tag_invoke` for the run_with CPO.
 *
 * @see run_with
 */
using detail::cpo_run_with::run_with_t;

} // namespace v1
} // namespace computation
} // namespace concore