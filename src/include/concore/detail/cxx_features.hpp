#pragma once

#include "concore/detail/platform.hpp"

#ifdef __has_include
#if __has_include(<version>)
#include <version>
#endif
#endif

#if USE_CXX2020 && defined(__cpp_concepts)
#define CONCORE_CXX_HAS_CONCEPTS 1
#else
#define CONCORE_CXX_HAS_CONCEPTS 0
#endif

#if USE_CXX2020 && defined(__cpp_lib_concepts)
#define CONCORE_CXX_HAS_LIB_CONCEPTS 1
#else
#define CONCORE_CXX_HAS_LIB_CONCEPTS 0
#endif

#if USE_CXX2020 && defined(__cpp_concepts)
#define CONCORE_CXX_HAS_CONCEPTS 1
#else
#define CONCORE_CXX_HAS_CONCEPTS 0
#endif

#if USE_CXX2020 && defined(__cpp_impl_coroutine)
#define CONCORE_CXX_HAS_COROUTINES 1
#else
#define CONCORE_CXX_HAS_COROUTINES 0
#endif

#if USE_CXX2020 && defined(__cpp_lib_coroutine)
#define CONCORE_CXX_HAS_LIB_COROUTINES 1
#else
#define CONCORE_CXX_HAS_LIB_COROUTINES 0
#endif

// cppcheck-suppress preprocessorErrorDirective
#if USE_CXX2020 && defined(__has_include) && __has_include(<experimental/coroutine>)
#include <experimental/coroutine>
#define CONCORE_CXX_HAS_PARTIAL_COROUTINES 1
#else
#define CONCORE_CXX_HAS_PARTIAL_COROUTINES 0
#endif

#if USE_CXX2020 && defined(__cpp_modules)
#define CONCORE_CXX_HAS_MODULES 1
#else
#define CONCORE_CXX_HAS_MODULES 0
#endif

#if USE_CXX2020 && !CONCORE_CXX_HAS_MODULES && CONCORE_CPP_COMPILER(clang) && __clang_major__ == 10
#include <experimental/coroutine>
#define CONCORE_CXX_HAS_PARTIAL_MODULES 1
#else
#define CONCORE_CXX_HAS_PARTIAL_MODULES 0
#endif

#if CONCORE_CXX_HAS_CONCEPTS
#define CONCORE_CONCEPT_TYPENAME(x) x
#else
#define CONCORE_CONCEPT_TYPENAME(x) typename
#endif