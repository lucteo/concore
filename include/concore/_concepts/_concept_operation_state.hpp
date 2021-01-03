#pragma once

#include <concore/detail/concept_macros.hpp>
#include <concore/detail/extra_type_traits.hpp>

#include <concore/_cpo/_cpo_start.hpp>

#if CONCORE_CXX_HAS_CONCEPTS
#include <concepts>
#endif

namespace concore {
inline namespace v1 {

#if CONCORE_CXX_HAS_CONCEPTS

template <typename OpState>
concept operation_state = std::destructible<OpState>&& std::is_object_v<OpState>&& requires(
        OpState& op) {
    { concore::start(op) }
    noexcept;
};

#else

template <typename OpState>
CONCORE_CONCEPT_OR_BOOL(operation_state) = std::is_destructible_v<OpState>&& std::is_object_v<
        OpState>&& detail::cpo_start::has_start<concore::detail::remove_cvref_t<OpState>>;

#endif

} // namespace v1
} // namespace concore
