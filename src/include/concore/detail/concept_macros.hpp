#pragma once

#include <concore/detail/cxx_features.hpp>
#include <type_traits>

namespace concore {

namespace detail {

//! Introduces a value of the given type. Can be used with vaargs types.
#define CONCORE_DECLVAL(...) static_cast<__VA_ARGS__ (*)() noexcept>(nullptr)()
//! Introduces a value that is a reference of the given type
#define CONCORE_DECLVALREF(...) *static_cast<__VA_ARGS__*>(nullptr)

//! Helper macro to pass lists of tokens as arguments
#define CONCORE_LIST(...) __VA_ARGS__

#if CONCORE_CXX_HAS_CONCEPTS
//! Defines a concept, or, if not available, a constexpr bool variable
//! Concept name expected after this macro
#define CONCORE_CONCEPT_OR_BOOL concept

//! Defines a concept-constrained typename, or, if not available, a simple typename
#define CONCORE_CONCEPT_OR_TYPENAME(name) name

//! Defines a concept (or a bool variable) with the result of a 'requires' constraint.
//! This is compatible with compilers that do not support concepts.
#define CONCORE_DEF_REQUIRES(                                                                      \
        varName, tparams, tnames, conceptRequiresExpr, nonConceptRequiresExpr)                     \
    template <tparams>                                                                             \
    concept varName = conceptRequiresExpr

//! Same as CONCORE_DEF_REQUIRES, but allows checking if the expression is noexcept.
//! For the "concepts" implementation, this needs to be handled inside the 'conceptRequiresExpr'
//! argument.
//! For the "non-concepts" implementation, this is handled inside the macro.
#define CONCORE_DEF_REQUIRES_NOEXCEPT(                                                             \
        varName, tparams, tnames, conceptRequiresExpr, nonConceptRequiresExpr)                     \
    template <tparams>                                                                             \
    concept varName = conceptRequiresExpr

//! A template heading with an enabling condition
#define CONCORE_TEMPLATE_COND(tparams, cond)                                                       \
    template <tparams>                                                                             \
    requires(cond)

#else

#define CONCORE_CONCEPT_OR_BOOL inline constexpr bool

#define CONCORE_CONCEPT_OR_TYPENAME(name) typename

#define CONCORE_DEF_REQUIRES(                                                                      \
        varName, tparams, tnames, conceptRequiresExpr, nonConceptRequiresExpr)                     \
    template <typename...>                                                                         \
    struct stag_##varName {};                                                                      \
    template <tparams, typename = decltype(nonConceptRequiresExpr)>                                \
    char tester_##varName(stag_##varName<tnames>*);                                                \
    double tester_##varName(...);                                                                  \
    template <tparams>                                                                             \
    CONCORE_CONCEPT_OR_BOOL varName =                                                              \
            sizeof(tester_##varName(static_cast<stag_##varName<tnames>*>(nullptr))) == 1;

#define CONCORE_DEF_REQUIRES_NOEXCEPT(                                                             \
        varName, tparams, tnames, conceptRequiresExpr, nonConceptRequiresExpr)                     \
    template <typename...>                                                                         \
    struct stag_##varName {};                                                                      \
    template <tparams, typename = decltype(nonConceptRequiresExpr)>                                \
    char tester_##varName(stag_##varName<tnames>*) noexcept(noexcept(nonConceptRequiresExpr));     \
    double tester_##varName(...) noexcept(false);                                                  \
    template <tparams>                                                                             \
    CONCORE_CONCEPT_OR_BOOL varName =                                                              \
            noexcept(tester_##varName(static_cast<stag_##varName<tnames>*>(nullptr)));

#define CONCORE_TEMPLATE_COND(tparams, cond) template <tparams, std::enable_if_t<cond, int> = 0>

#endif

} // namespace detail
} // namespace concore
