#pragma once

#include <concore/_cpo/_tag_invoke.hpp>

namespace concore {

namespace detail {
namespace cpo_set_error {

inline const struct set_error_t final {
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename R, typename E), //
            (tag_invocable<set_error_t, R, E>))
    void operator()(R&& r, E&& e) const                          //
            noexcept(nothrow_tag_invocable<set_error_t, R, E>) { //
        tag_invoke(set_error_t{}, (R &&) r, (E &&) e);
    }
} set_error{};

template <typename R, typename E>
CONCORE_CONCEPT_OR_BOOL(has_set_error) = tag_invocable<set_error_t, R, E>;

} // namespace cpo_set_error
} // namespace detail

inline namespace v1 {

using detail::cpo_set_error::set_error;
using detail::cpo_set_error::set_error_t;

} // namespace v1
} // namespace concore