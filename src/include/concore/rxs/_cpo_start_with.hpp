#pragma once

#include <concore/detail/cxx_features.hpp>
#include <concore/detail/extra_type_traits.hpp>
#include <concore/detail/concept_macros.hpp>

#if CONCORE_CXX_HAS_CONCEPTS
#include <concepts>
#endif
#include <type_traits>

namespace concore {

namespace rxs {

namespace detail {
namespace cpo_start_with {

CONCORE_DEF_REQUIRES(meets_tag_invoke,                                               //
        CONCORE_LIST(typename Tag, typename C, typename R), CONCORE_LIST(Tag, C, R), //
        (requires(C&& c, R&& r) { tag_invoke(Tag{}, (C &&) c, (R &&) r); }),         // concepts
        tag_invoke(Tag{}, CONCORE_DECLVAL(C), CONCORE_DECLVAL(R))                    // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_inner_fun,                             //
        CONCORE_LIST(typename C, typename R), CONCORE_LIST(C, R), //
        (requires(C&& c, R&& r) { c.start_with((R &&) r); }),     // concepts
        CONCORE_DECLVAL(C).start_with(CONCORE_DECLVAL(R))         // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_outer_fun,                                 //
        CONCORE_LIST(typename C, typename R), CONCORE_LIST(C, R),     //
        (requires(C&& c, R&& r) { start_with((C &&) c, (R &&) r); }), // concepts
        start_with(CONCORE_DECLVAL(C), CONCORE_DECLVAL(R))            // pre-concepts
);

template <typename Tag, typename C, typename R>
CONCORE_CONCEPT_OR_BOOL(has_tag_invoke) = meets_tag_invoke<Tag, C, R>;

template <typename Tag, typename C, typename R>
CONCORE_CONCEPT_OR_BOOL(has_inner_fun) = !has_tag_invoke<Tag, C, R> && meets_inner_fun<C, R>;

template <typename Tag, typename C, typename R>
CONCORE_CONCEPT_OR_BOOL(has_outer_fun) = !has_tag_invoke<Tag, C, R> &&
                                         !has_inner_fun<Tag, C, R> && meets_outer_fun<C, R>;

inline const struct start_with_t final {
    CONCORE_TEMPLATE_COND(
            CONCORE_LIST(typename C, typename R), (has_tag_invoke<start_with_t, C, R>))
    void operator()(C&& c, R&& r) const                                 //
            noexcept(noexcept(tag_invoke(*this, (C &&) c, (R &&) r))) { //
        tag_invoke(start_with_t{}, (C &&) c, (R &&) r);
    }
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename C, typename R), (has_inner_fun<start_with_t, C, R>))
    void operator()(C&& c, R&& r) const                           //
            noexcept(noexcept(((C &&) c).start_with((R &&) r))) { //
        ((C &&) c).start_with((R &&) r);
    }
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename C, typename R), (has_outer_fun<start_with_t, C, R>))
    void operator()(C&& c, R&& r) const                          //
            noexcept(noexcept(start_with((C &&) c, (R &&) r))) { //
        start_with((C &&) c, (R &&) r);
    }
} start_with{};

template <typename C, typename R>
CONCORE_CONCEPT_OR_BOOL(has_start_with) =
        meets_tag_invoke<start_with_t, C, R> || meets_inner_fun<C, R> || meets_outer_fun<C, R>;

} // namespace cpo_start_with
} // namespace detail

inline namespace v1 {

/**
 * @brief CPO for void start_with(stream, receiver).
 *
 * @param stream    the stream that need to be started
 * @param receiver  the receiver that will get the notifications from the stream
 *
 * @details
 *
 * This CPO essentially introduces a functor object with the following signature:
 * @code{.cpp}
 *      template <typename S, typename Recv>
 *      void start_with(S stream, Recv receiver);
 * @endcode
 *
 * By using this CPO, we allow the user to define the `start_with` functions for a stream S in
 * three ways:
 * 1) by creating a tag_invoke specialization
 * 2) by adding `start_with` as a method inside C
 * 3) by adding `start_with(S, R)` as a free function near S
 *
 * Example for creating a tag_invoke specialization:
 * @code{.cpp}
 *      template <typename S, typename Recv>
 *      void start_with(start_with_t, S stream, Recv receiver);
 * @endcode
 *
 * Example for using a method inside class C
 * @code{.cpp}
 *      struct S {
 *          ...
 *          template <typename Recv>
 *          void start_with(Recv receiver);
 *      };
 * @endcode
 *
 * Example for using a free function outside S
 * @code{.cpp}
 *      struct S;
 *
 *      template <typename Recv>
 *      void start_with(S stream, Recv receiver);
 * @endcode
 *
 * This is needed for the @ref stream concept. It is also used in the library to start stream
 * processing. The end-user typically doesn't have to call this.
 *
 * @see stream
 */
using detail::cpo_start_with::start_with;

/**
 * @brief Tag type to be used to specialize `tag_invoke` for the start_with CPO.
 *
 * @see start_with
 */
using detail::cpo_start_with::start_with_t;

} // namespace v1
} // namespace rxs
} // namespace concore