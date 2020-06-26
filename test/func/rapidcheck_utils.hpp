#pragma once

#include <rapidcheck.h>

//! Define an rc::Arbitrary generator for a type
#define DEFINE_RC_ARBITRARY(type, fun_call)                                                        \
    namespace rc {                                                                                 \
    template <>                                                                                    \
    struct Arbitrary<type> {                                                                       \
        static Gen<type> arbitrary() { return fun_call; }                                          \
    };                                                                                             \
    }

//! Similar to a Catch2 SECTION, this is helpful to be used with rapidcheck properties
//! Can be used instead of rc::check.
//! Sometimes double parenthesis are needed around testable.
#define PROPERTY(testable)                                                                         \
    {                                                                                              \
        bool property_ok = rc::check(testable);                                                    \
        REQUIRE(property_ok);                                                                      \
    }
