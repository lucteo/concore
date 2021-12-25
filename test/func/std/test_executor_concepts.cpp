#include <catch2/catch.hpp>
#include <concore/execution_old.hpp>
#include <concore/spawn.hpp>
#include <concore/global_executor.hpp>
#include <concore/any_executor.hpp>
#include <concore/delegating_executor.hpp>
#include <concore/inline_executor.hpp>
#include <concore/thread_pool.hpp>

#include <concore/serializer.hpp>
#include <concore/rw_serializer.hpp>
#include <concore/n_serializer.hpp>

template <typename T>
void ensure_executor() {
    static_assert(concore::executor<T>, "Given type is not an executor");
}

template <typename T>
void ensure_task_executor() {
    static_assert(concore::task_executor<T>, "Given type is not a type_executor");
}

TEST_CASE("various executor types model the executor concept", "[execution][concepts]") {
    // main
    ensure_executor<concore::global_executor>();
    ensure_executor<concore::spawn_executor>();
    ensure_executor<concore::spawn_continuation_executor>();

    // generic
    ensure_executor<concore::any_executor>();

    // other
    ensure_executor<concore::delegating_executor>();
    ensure_executor<concore::inline_executor>();
    ensure_executor<concore::static_thread_pool::executor_type>();

    // serializers
    ensure_executor<concore::serializer>();
    ensure_executor<concore::n_serializer>();
    ensure_executor<concore::rw_serializer::reader_type>();
    ensure_executor<concore::rw_serializer::writer_type>();
}

TEST_CASE("various executor types model the task_executor concept", "[execution][concepts]") {
    // main
    ensure_task_executor<concore::global_executor>();
    ensure_task_executor<concore::spawn_executor>();
    ensure_task_executor<concore::spawn_continuation_executor>();

    // generic
    ensure_task_executor<concore::any_executor>();

    // other
    ensure_task_executor<concore::delegating_executor>();
    ensure_task_executor<concore::inline_executor>();
    ensure_task_executor<concore::static_thread_pool::executor_type>();

    // serializers
    ensure_task_executor<concore::serializer>();
    ensure_task_executor<concore::n_serializer>();
    ensure_task_executor<concore::rw_serializer::reader_type>();
    ensure_task_executor<concore::rw_serializer::writer_type>();
}
