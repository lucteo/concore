#pragma once

#include <concore/detail/cxx_features.hpp>

namespace concore {

namespace detail {

template <typename T>
T valOfType();

#if CONCORE_CXX_HAS_CONCEPTS
//! Defines a concept, or, if not available, ca constexpr bool variable
#define CONCORE_CONCEPT_OR_BOOL(name) concept name
//! Defines a concept with a requires case with 2 template params: T1 and T2.
//! To be used with CONCORE_NONCONCEPT_REQUIRES_CASE2.
#define CONCORE_CONCEPT_REQUIRES_CASE2(conceptName, requiresClause)                                \
    template <typename T1, typename T2>                                                            \
    concept conceptName = requiresClause
//! Defines a concept with a requires case with 3 template params: T1, T2 and T3.
//! To be used with CONCORE_NONCONCEPT_REQUIRES_CASE3.
#define CONCORE_CONCEPT_REQUIRES_CASE3(conceptName, requiresClause)                                \
    template <typename T1, typename T2, typename T3>                                               \
    concept conceptName = requiresClause

//! This should be used in the case there are no concepts to define an alternative expression by
//! using boolean expressions. For 2 template params: T1 and T2.
//! To be used with CONCORE_CONCEPT_REQUIRES_CASE2.
#define CONCORE_NONCONCEPT_REQUIRES_CASE2(tagTypeName, requiresExpr) /*nothing*/
//! This should be used in the case there are no concepts to define an alternative expression by
//! using boolean expressions. For 2 template params: T1, T2 and T3.
//! To be used with CONCORE_CONCEPT_REQUIRES_CASE3.
#define CONCORE_NONCONCEPT_REQUIRES_CASE3(tagTypeName, requiresExpr) /*nothing*/
//! Defines common specialization code when we don't have concepts
#define CONCORE_NONCONCEPT_REQUIRES_DEFAULTCASE() /*nothing*/

//! To be added at template parameter specializing over a bool condition
#define CONCORE_NONCONCEPT_EXTRA_TEMPLATE_COND(cond) int = 0
//! To be added at template parameter specializing with requires clauses
#define CONCORE_CONCEPT_REQUIRES_COND(cond) requires cond

#else

#define CONCORE_CONCEPT_OR_BOOL(name) inline constexpr bool name
#define CONCORE_CONCEPT_REQUIRES_CASE2(conceptName, requiresClause) /*nothing*/
#define CONCORE_CONCEPT_REQUIRES_CASE3(conceptName, requiresClause) /*nothing*/

#define CONCORE_NONCONCEPT_REQUIRES_CASE2(varName, requiresExpr)                                   \
    template <typename...>                                                                         \
    struct specialization_tag_##varName {};                                                        \
    template <typename T1, typename T2, typename = decltype(requiresExpr)>                         \
    char tester(specialization_tag_##varName<T1, T2>*);                                            \
    template <typename T1, typename T2>                                                            \
    inline constexpr bool(varName) =                                                               \
            sizeof(tester(static_cast<specialization_tag_##varName<T1, T2>*>(nullptr))) == 1;

#define CONCORE_NONCONCEPT_REQUIRES_CASE3(varName, requiresExpr)                                   \
    template <typename...>                                                                         \
    struct specialization_tag_##varName {};                                                        \
    template <typename T1, typename T2, typename T3, typename = decltype(requiresExpr)>            \
    char tester(specialization_tag_##varName<T1, T2, T3>*);                                        \
    template <typename T1, typename T2, typename T3>                                               \
    inline constexpr bool(varName) =                                                               \
            sizeof(tester(static_cast<specialization_tag_##varName<T1, T2, T3>*>(nullptr))) == 1;

#define CONCORE_NONCONCEPT_REQUIRES_DEFAULTCASE()                                                  \
    double tester(...);                                                                            \
    template <typename T>                                                                          \
    inline constexpr bool satisfiesRequires() {                                                    \
        return sizeof(tester(static_cast<T*>(nullptr))) == 1;                                      \
    }

#define CONCORE_NONCONCEPT_EXTRA_TEMPLATE_COND(cond) std::enable_if_t<cond, int> = 0
#define CONCORE_CONCEPT_REQUIRES_COND(cond) /*nothing*/

#endif

} // namespace detail
} // namespace concore
