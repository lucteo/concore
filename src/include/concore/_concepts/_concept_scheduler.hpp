#pragma once

#include <concore/detail/cxx_features.hpp>

#include <concore/_cpo/_cpo_schedule.hpp>

#include <type_traits>

#if CONCORE_CXX_HAS_CONCEPTS
#include <concepts>
#endif

namespace concore {

namespace detail {}; // namespace detail

inline namespace v1 {

#if CONCORE_CXX_HAS_CONCEPTS

template <typename S>
concept scheduler =                                          //
        (std::copy_constructible<std::remove_cvref_t<S>>)    //
        &&(std::equality_comparable<std::remove_cvref_t<S>>) //
        &&(requires(S&& s) {                                 //
            concore::schedule((S &&) s);
        });

#else

template <typename S>
CONCORE_CONCEPT_OR_BOOL scheduler =                                               //
        std::is_nothrow_move_constructible<detail::remove_cvref_t<S>>::value      //
        && (std::is_nothrow_copy_constructible<detail::remove_cvref_t<S>>::value) //
        /*std::equality_comparable*/                                              //
        && (detail::cpo_schedule::has_schedule<detail::remove_cvref_t<S>>);

#endif

} // namespace v1

} // namespace concore
