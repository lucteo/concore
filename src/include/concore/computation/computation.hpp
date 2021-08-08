#pragma once

#include <concore/computation/_cpo_run_with.hpp>
#include <concore/detail/cxx_features.hpp>
#include <concore/detail/extra_type_traits.hpp>
#include <concore/detail/concept_macros.hpp>

#if CONCORE_CXX_HAS_CONCEPTS
#include <concepts>
#endif
#include <type_traits>

namespace concore {

namespace computation {

namespace detail {

template <typename R>
struct receiver_archetype {
    void set_value(R) {}
    void set_done() noexcept {}
    void set_error(std::exception_ptr) noexcept {}

    friend inline bool operator==(receiver_archetype, receiver_archetype) { return false; }
    friend inline bool operator!=(receiver_archetype, receiver_archetype) { return true; }
};

template <>
struct receiver_archetype<void> {
    void set_value() {}
    void set_done() noexcept {}
    void set_error(std::exception_ptr) noexcept {}

    friend inline bool operator==(receiver_archetype, receiver_archetype) { return false; }
    friend inline bool operator!=(receiver_archetype, receiver_archetype) { return true; }
};

struct error_type {};

error_type computation_value_type_tester(...);
template <typename C, typename R = typename C::value_type>
R computation_value_type_tester(C*);

template <typename C>
using computation_value_type = decltype(computation_value_type_tester(static_cast<C*>(nullptr)));

template <typename C, typename R = computation_value_type<C>>
CONCORE_CONCEPT_OR_BOOL(computation_impl) =                              //
        (std::is_copy_constructible_v<C>)                                //
        &&(!std::is_same_v<R, error_type>)                               //
        &&(detail::cpo_run_with::has_run_with<C, receiver_archetype<R>>) //
        ;

} // namespace detail

inline namespace v1 {

/**
 * @brief Concept modeling types that represent computations.
 *
 * @tparam C The type to check if it matches the concept
 *
 * @details
 *
 * A type T models computation if objects of type T can represent computations that result in
 * a value of a given type, or void (in the absence of errors). The type of value computed by the
 * computation will be indicated by the inner type `value_type`; this can be void. Semantically, one
 * can perform two operations with such a computation object:
 * - start the computation (i.e., executing all the steps to produce a expected value)
 * - make use of the computed value (in the absence of errors), by being able to connect it to
 * algorithms that extract the resulting value out of the computation.
 *
 * The computation either succeeds in producing a value of the expected type, or an error has
 * occurred. If the execution was never started (i.e., cancelled), then the computation will assume
 * a `task_cancelled` exception was thrown.
 *
 * We are making one important assumption: the computation can be started maximum once. From this it
 * follows that a computation can produce maximum one value. Also, by the way computations are
 * constructed, the value of the computation cannot be possible accessed until the computation is
 * performed.
 *
 * To connect the computation with another entity and to extract the value out of the computation,
 * one needs to pass in a receiver object, of the proper value type. This is typically done at the
 * library level, and the end-user should not care about connecting the computation directly.
 *
 * The computation needs to have a run_with(receiver<value_type>) CPO. This will start the
 * computation represented by the actual object and, when completed (either successfully or by
 * error) will call the receiver passed in. The receiver passed in must match the value type of the
 * current computation.
 *
 * Constraints enforced on the type C modeling our concept:
 * - is copy constructable
 * - has an inner type `value_type` (representing the resulting type of the computation)
 * - CPO call run_with(C, Recv) is valid, where Recv is a receiver type with appropriate value type
 *
 * Example of a computation type:
 * @code{.cpp}
 *      struct fixed_value_computation {
 *          using value_type = int;
 *
 *          explicit fixed_value_computation(int val): val_(val) {}
 *
 *          template <typename Recv>
 *          void run_with(Recv recv) {
 *              concore::set_value((Recv&&) recv, val_);
 *          }
 *      private:
 *          int val_;
 *      };
 * @endcode
 *
 * @see task_cancelled, receiver_of
 */
template <typename C>
CONCORE_CONCEPT_OR_BOOL(computation) = detail::computation_impl<C>;

//! Type traits that extracts the value type from a computation. Returns an error type if the given
//! type is not a computation.
using detail::computation_value_type;

} // namespace v1
} // namespace computation
} // namespace concore