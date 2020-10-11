#include <concore/global_executor.hpp>
#include <concore/dispatch_executor.hpp>
#ifdef CONCORE_USE_TBB
#include <concore/tbb_executor.hpp>
#include <tbb/task_scheduler_init.h>
#endif
#include <concore/immediate_executor.hpp>
#include <concore/spawn.hpp>
#include <concore/serializer.hpp>
#include <concore/n_serializer.hpp>
#include <concore/rw_serializer.hpp>
#include <concore/profiling.hpp>

#include "test_common/task_countdown.hpp"

#include <benchmark/benchmark.h>
#include <thread>

template <int N>
int get_N() {
    return N;
}

using ptr_fun_t = int (*)();

ptr_fun_t ptr_fun = nullptr;

void set_random_ptr_fun() {
    // Set the function to be called
    int n = rand() % 10;
    switch (n) {
    case 0:
        ptr_fun = &get_N<0>;
        break;
    case 1:
        ptr_fun = &get_N<1>;
        break;
    case 2:
        ptr_fun = &get_N<2>;
        break;
    case 3:
        ptr_fun = &get_N<3>;
        break;
    case 4:
        ptr_fun = &get_N<4>;
        break;
    case 5:
        ptr_fun = &get_N<5>;
        break;
    case 6:
        ptr_fun = &get_N<6>;
        break;
    case 7:
        ptr_fun = &get_N<7>;
        break;
    case 8:
        ptr_fun = &get_N<8>;
        break;
    case 9:
        ptr_fun = &get_N<9>;
        break;
    }
}

template <typename E>
void do_iter(int n, E& executor, std::chrono::time_point<std::chrono::high_resolution_clock>& end) {
    CONCORE_PROFILING_FUNCTION();
    // CONCORE_PROFILING_SET_TEXT_FMT(32, "n=%d", n);
    if (n > 0) {
        benchmark::DoNotOptimize(executor);
        executor([n, &executor, &end]() {
            // Call us with a decremented count
            do_iter(n - 1, executor, end);
        });
    } else
        end = std::chrono::high_resolution_clock::now();
}

template <typename E>
static void test_latency(E executor, benchmark::State& state) {
    const int num_tasks = state.range(0);

    // Add a task to the executor, just to ensure that the executor is warmed up
    executor([]() { benchmark::DoNotOptimize(std::thread::hardware_concurrency()); });
    std::this_thread::sleep_for(200ms);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        std::chrono::time_point<std::chrono::high_resolution_clock> end =
                std::chrono::high_resolution_clock::now();
        auto start = std::chrono::high_resolution_clock::now();

        do_iter(num_tasks, executor, end);

        // Wait until we finish, and 'end' will be set
        while (end < start)
            std::this_thread::sleep_for(1us);
        std::this_thread::sleep_for(1us);

        // Report the time
        auto elapsed_seconds =
                std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        std::this_thread::sleep_for(10us);
    }
}

template <typename E>
static void test_latency_ser(E executor, benchmark::State& state) {
    const int num_tasks = state.range(0);

    // Add a task to the executor, just to ensure that the executor is warmed up
    executor([]() { benchmark::DoNotOptimize(std::thread::hardware_concurrency()); });
    std::this_thread::sleep_for(200ms);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        std::chrono::time_point<std::chrono::high_resolution_clock> end =
                std::chrono::high_resolution_clock::now();
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = num_tasks - 1; i >= 0; i--) {
            executor([i, &end]() {
                CONCORE_PROFILING_SCOPE_N("ser task");
                CONCORE_PROFILING_SET_TEXT_FMT(32, "%d", i);
                if (i == 0)
                    end = std::chrono::high_resolution_clock::now();
            });
        }

        // Wait until we finish, and 'end' will be set
        while (end < start)
            std::this_thread::sleep_for(1us);
        std::this_thread::sleep_for(1us);

        // Report the time
        auto elapsed_seconds =
                std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        std::this_thread::sleep_for(10us);
    }
}

static void BM_latency_fun_call_1(benchmark::State& state) {
    // Ensure we have a function set; use random, to trick the compiler in optimizing it out
    set_random_ptr_fun();
    std::this_thread::sleep_for(200ms);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        auto start = std::chrono::high_resolution_clock::now();

        // Measure the time it takes to call a function pointer
        benchmark::DoNotOptimize((*ptr_fun)());

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds =
                std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
        // std::this_thread::sleep_for(10us);
    }
}

static void BM_latency_fun_call_n(benchmark::State& state) {
    const int num_tasks = state.range(0);

    // Ensure we have a function set; use random, to trick the compiler in optimizing it out
    set_random_ptr_fun();
    std::this_thread::sleep_for(200ms);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        auto start = std::chrono::high_resolution_clock::now();

        // Measure the time it takes to call a function pointer
        for (int i = 0; i < num_tasks; i++) {
            benchmark::DoNotOptimize((*ptr_fun)());
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds =
                std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
    }
}

static void BM_latency_global(benchmark::State& state) {
    CONCORE_PROFILING_SCOPE_N("test global_executor");
    test_latency(concore::global_executor, state);
}

static void BM_latency_spawn(benchmark::State& state) {
    CONCORE_PROFILING_SCOPE_N("test spawn");
    test_latency(concore::spawn_executor, state);
}

static void BM_latency_spawn_cont(benchmark::State& state) {
    CONCORE_PROFILING_SCOPE_N("test spawn continuation");
    test_latency(concore::spawn_continuation_executor, state);
}

static void BM_latency_dispatch(benchmark::State& state) {
#if CONCORE_USE_LIBDISPATCH
    CONCORE_PROFILING_SCOPE_N("test dispatch_executor");
    test_latency(concore::dispatch_executor, state);
#endif
}

static void BM_latency_tbb(benchmark::State& state) {
#if CONCORE_USE_TBB
    CONCORE_PROFILING_SCOPE_N("test tbb_executor");
    tbb::task_scheduler_init init(1 + std::thread::hardware_concurrency()); // make it a fair comp
    test_latency(concore::tbb_executor, state);
#endif
}

static void BM_latency_immediate(benchmark::State& state) {
    CONCORE_PROFILING_SCOPE_N("test immediate_executor");
    test_latency(concore::immediate_executor, state);
}

static void BM_latency_serializer(benchmark::State& state) {
    CONCORE_PROFILING_SCOPE_N("test serializer");
    test_latency_ser(concore::serializer(concore::spawn_continuation_executor), state);
}

static void BM_latency_n_serializer(benchmark::State& state) {
    CONCORE_PROFILING_SCOPE_N("test n_serializer");
    test_latency_ser(concore::n_serializer(1, concore::spawn_continuation_executor), state);
}

static void BM_latency_rw_serializer(benchmark::State& state) {
    CONCORE_PROFILING_SCOPE_N("test rw_serializer");
    test_latency_ser(concore::rw_serializer(concore::spawn_continuation_executor).writer(), state);
}

static void BM___(benchmark::State& /*state*/) {}
#define BENCHMARK_PAUSE() BENCHMARK(BM___)

#define BENCHMARK_CASE1(fun) BENCHMARK(fun)->UseManualTime()
#define BENCHMARK_CASE2(fun)                                                                       \
    BENCHMARK(fun)->UseManualTime()->Unit(benchmark::kMicrosecond)->Arg(10000);

BENCHMARK_CASE1(BM_latency_fun_call_1);
BENCHMARK_PAUSE();

BENCHMARK_CASE2(BM_latency_fun_call_n);
BENCHMARK_CASE2(BM_latency_global);
BENCHMARK_CASE2(BM_latency_spawn);
BENCHMARK_CASE2(BM_latency_spawn_cont);
BENCHMARK_CASE2(BM_latency_dispatch);
BENCHMARK_CASE2(BM_latency_tbb);
BENCHMARK_CASE2(BM_latency_immediate);
BENCHMARK_CASE2(BM_latency_serializer);
BENCHMARK_CASE2(BM_latency_n_serializer);
BENCHMARK_CASE2(BM_latency_rw_serializer);
BENCHMARK_PAUSE();

BENCHMARK_CASE2(BM_latency_fun_call_n);
BENCHMARK_CASE2(BM_latency_global);
BENCHMARK_CASE2(BM_latency_spawn);
BENCHMARK_CASE2(BM_latency_spawn_cont);
BENCHMARK_CASE2(BM_latency_dispatch);
BENCHMARK_CASE2(BM_latency_tbb);
BENCHMARK_CASE2(BM_latency_immediate);
BENCHMARK_CASE2(BM_latency_serializer);
BENCHMARK_CASE2(BM_latency_n_serializer);
BENCHMARK_CASE2(BM_latency_rw_serializer);
BENCHMARK_PAUSE();

BENCHMARK_MAIN();
