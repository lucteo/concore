#pragma once

#include <concore/detail/concept_macros.hpp>

namespace concore {
namespace std_execution {

namespace detail {
namespace cpo_set_done {

CONCORE_DEF_REQUIRES(meets_tag_invoke,                                //
        CONCORE_LIST(typename Tag, typename R), CONCORE_LIST(Tag, R), //
        (requires(R&& r) { tag_invoke(Tag{}, (R &&) r); }),           // concepts
        tag_invoke(Tag{}, CONCORE_DECLVAL(R))                         // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_inner_fun,        //
        typename R, R,                       //
        (requires(R&& r) { r.set_done(); }), // concepts
        CONCORE_DECLVAL(R).set_done()        // pre-concepts
);
CONCORE_DEF_REQUIRES(meets_outer_fun,              //
        typename R, R,                             //
        (requires(R&& r) { set_done((R &&) r); }), // concepts
        set_done(CONCORE_DECLVAL(R))               // pre-concepts
);

template <typename Tag, typename R>
CONCORE_CONCEPT_OR_BOOL(has_tag_invoke) = meets_tag_invoke<Tag, R>;

template <typename Tag, typename R>
CONCORE_CONCEPT_OR_BOOL(has_inner_fun) = !has_tag_invoke<Tag, R> && meets_inner_fun<R>;

template <typename Tag, typename R>
CONCORE_CONCEPT_OR_BOOL(has_outer_fun) = !has_tag_invoke<Tag, R> &&
                                         !has_inner_fun<Tag, R> && meets_outer_fun<R>;

inline const struct set_done_t final {
    CONCORE_TEMPLATE_COND(typename R, (has_tag_invoke<set_done_t, R>))
    void operator()(R&& r) const                              //
            noexcept(noexcept(tag_invoke(*this, (R &&) r))) { //
        tag_invoke(set_done_t{}, (R &&) r);
    }
    CONCORE_TEMPLATE_COND(typename R, (has_inner_fun<set_done_t, R>))
    void operator()(R&& r) const                        //
            noexcept(noexcept(((R &&) r).set_done())) { //
        ((R &&) r).set_done();
    }
    CONCORE_TEMPLATE_COND(typename R, (has_outer_fun<set_done_t, R>))
    void operator()(R&& r) const                     //
            noexcept(noexcept(set_done((R &&) r))) { //
        set_done((R &&) r);
    }
} set_done{};

template <typename R>
CONCORE_CONCEPT_OR_BOOL(
        has_set_done) = meets_tag_invoke<set_done_t, R> || meets_inner_fun<R> || meets_outer_fun<R>;

} // namespace cpo_set_done
} // namespace detail

inline namespace v1 {

/**
 * @brief   Function-like object that can be used to signal stop to receivers
 *
 * @tparam  R       The type of receiver we want to use; must model `receiver`
 *
 * @param   r       The receiver object that is signaled about sender's stop signal
 *
 * This is called by a sender whenever the sender is stopped.
 *
 * `receiver<R>` must be true.
 */
using detail::cpo_set_done::set_done;

/**
 * @brief   Type to use for customization point for set_done
 *
 * This can be used for types that do not directly model the receiver concepts. One can define a
 * `tag_invoke` customization point to make the type be a receiver.
 *
 * For a type to be receiver, it needs to have the following customization points:
 *  - `tag_invoke(set_value_t, receiver, ...)`
 *  - `tag_invoke(set_done_t, receiver)`
 *  - `tag_invoke(set_error_t, receiver, err)`
 *
 * @see     set_done
 */
using detail::cpo_set_done::set_done_t;

} // namespace v1
} // namespace std_execution
} // namespace concore