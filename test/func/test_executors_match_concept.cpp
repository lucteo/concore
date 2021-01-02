#include <concore/detail/cxx_features.hpp>

#if CONCORE_CXX_HAS_CONCEPTS

#include <catch2/catch.hpp>
#include <concore/execution.hpp>
#include <concore/inline_executor.hpp>
#include <concore/global_executor.hpp>
#include <concore/spawn.hpp>
#include <concore/delegating_executor.hpp>
#include <concore/any_executor.hpp>
#include <concore/serializer.hpp>
#include <concore/n_serializer.hpp>
#include <concore/rw_serializer.hpp>

using concore::executor;

TEST_CASE("inline_executor is an executor", "[executor][concepts]") {

    static_assert(executor<concore::inline_executor>);
    static_assert(executor<concore::global_executor>);
    static_assert(executor<concore::spawn_executor>);
    static_assert(executor<concore::spawn_continuation_executor>);
    static_assert(executor<concore::delegating_executor>);
    static_assert(executor<concore::any_executor>);
    static_assert(executor<concore::serializer>);
    static_assert(executor<concore::n_serializer>);
    static_assert(executor<concore::rw_serializer::reader_type>);
    static_assert(executor<concore::rw_serializer::writer_type>);
}

#endif
