#pragma once

#include <concore/detail/concept_macros.hpp>
#include <concore/detail/extra_type_traits.hpp>

namespace concore {

namespace detail {

//! Concept that checks if one can invoke the operation represented by the given Tag with the given
//! argument types.
//! Example: If R is a receiver and accepts type 'int', then
//! tag_invocable<concore::set_value_t, R, int> == true
CONCORE_DEF_REQUIRES(tag_invocable,                                               //
        CONCORE_LIST(typename Tag, typename... Args), CONCORE_LIST(Tag, Args...), //
        (requires(Args&&... args) { tag_invoke(Tag{}, (Args &&) args...); }),     // concepts
        tag_invoke(Tag{}, CONCORE_DECLVAL(Args)...)                               // pre-concepts
);

//! Concept that checks if some operation is tag_invocable, and the operation doesn't throw
template <typename Tag, typename... Args>
CONCORE_CONCEPT_OR_BOOL(nothrow_tag_invocable) = //
        tag_invocable<Tag, Args...>              //
        && (noexcept(tag_invoke(Tag{}, CONCORE_DECLVAL(Args)...)));

//! Helper to get the result of a tag_invoke operation, wrapped
template <typename Tag, typename... Args>
using tag_invoke_result = concore::detail::invoke_result<Tag, Args...>;
//! Helper to get the result of a tag_invoke operation, without wrapping
template <typename Tag, typename... Args>
using tag_invoke_result_t = concore::detail::invoke_result_t<Tag, Args...>;

} // namespace detail
} // namespace concore