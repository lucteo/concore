#pragma once

#include <concore/detail/concept_macros.hpp>
#include <concore/detail/extra_type_traits.hpp>

#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_done.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>

#if CONCORE_CXX_HAS_CONCEPTS
#include <concepts>
#endif
#include <exception>
#include <utility>

namespace concore {

#if !CONCORE_CXX_HAS_CONCEPTS
namespace detail {

using cpo_set_done::has_set_done;
using cpo_set_error::has_set_error;
using cpo_set_value::has_set_value;

} // namespace detail
#endif

inline namespace v1 {

#if CONCORE_CXX_HAS_CONCEPTS

template <typename T, typename E = std::exception_ptr>
concept receiver =                                            //
        std::move_constructible<std::remove_cvref_t<T>>       //
        && std::constructible_from<std::remove_cvref_t<T>, T> //
        &&(requires(std::remove_cvref_t<T>&& t, E&& e) {      //
            { concore::set_done(std::move(t)) }               //
            noexcept;                                         //
            { concore::set_error(std::move(t), (E &&) e) }    //
            noexcept;                                         //
        });

template <typename T, typename... Vs>
concept receiver_of =                                         //
        receiver<T>                                           //
        &&(requires(std::remove_cvref_t<T>&& t, Vs&&... vs) { //
            concore::set_value(std::move(t), (Vs &&) vs...);  //
        });

#else

template <typename T, typename E = std::exception_ptr>
CONCORE_CONCEPT_OR_BOOL receiver =                                                //
        (std::is_move_constructible<detail::remove_cvref_t<T>>::value)            //
        && (std::is_constructible<detail::remove_cvref_t<T>, T>::value)           //
        && (detail::nothrow_tag_invocable<set_done_t, detail::remove_cvref_t<T>>) //
        &&detail::nothrow_tag_invocable<set_error_t, detail::remove_cvref_t<T>, E>;

template <typename T, typename... Vs>
CONCORE_CONCEPT_OR_BOOL receiver_of =     //
        (receiver<T, std::exception_ptr>) //
        &&(detail::tag_invocable<set_value_t, detail::remove_cvref_t<T>, Vs...>);

#endif

} // namespace v1

} // namespace concore
