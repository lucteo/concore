#pragma once

#include <concore/_cpo/_tag_invoke.hpp>

namespace concore {

namespace detail {
namespace cpo_set_done {

inline const struct set_done_t final {
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename R), //
            (tag_invocable<set_done_t, R>))
    void operator()(R&& r) const                             //
            noexcept(nothrow_tag_invocable<set_done_t, R>) { //
        tag_invoke(set_done_t{}, (R &&) r);
    }
} set_done{};

template <typename R>
CONCORE_CONCEPT_OR_BOOL has_set_done = tag_invocable<set_done_t, R>;

} // namespace cpo_set_done
} // namespace detail

inline namespace v1 {

using detail::cpo_set_done::set_done;
using detail::cpo_set_done::set_done_t;

} // namespace v1
} // namespace concore