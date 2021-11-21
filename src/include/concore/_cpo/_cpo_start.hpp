#pragma once

#include <concore/_cpo/_tag_invoke.hpp>

namespace concore {

namespace detail {
namespace cpo_start {

inline const struct start_t final {
    CONCORE_TEMPLATE_COND(CONCORE_LIST(typename O), //
            (tag_invocable<start_t, O&>))
    void operator()(O& op) const                           //
            noexcept(nothrow_tag_invocable<start_t, O&>) { //
        tag_invoke(start_t{}, op);
    }
} start{};

template <typename O>
CONCORE_CONCEPT_OR_BOOL(has_start) = tag_invocable<start_t, O&>;

} // namespace cpo_start
} // namespace detail

inline namespace v1 {

using detail::cpo_start::start;
using detail::cpo_start::start_t;

} // namespace v1
} // namespace concore