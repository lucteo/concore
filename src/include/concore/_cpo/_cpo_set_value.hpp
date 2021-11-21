#pragma once

#include <concore/_cpo/_tag_invoke.hpp>

namespace concore {

namespace detail {
namespace cpo_set_value {

inline const struct set_value_t final {
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename R, typename... Vs), //
            (tag_invocable<set_value_t, R, Vs...>))
    void operator()(R&& r, Vs&&... vs) const                         //
            noexcept(nothrow_tag_invocable<set_value_t, R, Vs...>) { //
        tag_invoke(set_value_t{}, (R &&) r, (Vs &&) vs...);
    }
} set_value{};

template <typename R, typename... Vs>
CONCORE_CONCEPT_OR_BOOL has_set_value = tag_invocable<set_value_t, R, Vs...>;

} // namespace cpo_set_value
} // namespace detail

inline namespace v1 {

using detail::cpo_set_value::set_value;
using detail::cpo_set_value::set_value_t;

} // namespace v1
} // namespace concore