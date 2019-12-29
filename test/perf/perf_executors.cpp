#include <concore/global_executor.hpp>
#ifdef __APPLE__
#include <concore/dispatch_executor.hpp>
#endif
#ifdef CONCORE_USE_TBB
#include <concore/tbb_executor.hpp>
#include <tbb/task_scheduler_init.h>
#endif
#include <concore/immediate_executor.hpp>

#include "test_common/task_countdown.hpp"

#include <benchmark/benchmark.h>
#include <thread>

static uint64_t bad_fib(uint64_t n) { return n < 2 ?: bad_fib(n - 1) + bad_fib(n - 2); }

template <typename E>
static void execute_lots_of_tasks(E executor, benchmark::State& state) {
    const int num_tasks = state.range(0);
    const int problem_size = state.range(1);
    task_countdown tc{num_tasks};

    // Ensure that the executor is warmed up
    executor([problem_size]() { benchmark::DoNotOptimize(bad_fib(problem_size)); });
    std::this_thread::sleep_for(200ms);

    for (auto _ : state) {
        {
            CONCORE_PROFILING_SCOPE_N("perf iter");
            for (int i = 0; i < num_tasks; i++) {
                executor([&tc, problem_size]() {
                    CONCORE_PROFILING_SCOPE_N("my task");
                    benchmark::DoNotOptimize(bad_fib(problem_size));
                    tc.task_finished();
                });
            }
            tc.wait_for_all(10s);
        }
        state.PauseTiming();
        {
            CONCORE_PROFILING_SCOPE_N("iter end");
            tc.reset(num_tasks);
            std::this_thread::sleep_for(200ms);
        }
    }
}

static void BM_execute_lots_of_tasks_manual_threads(benchmark::State& state) {
    const int num_tasks = state.range(0);
    const int problem_size = state.range(1);
    task_countdown tc{num_tasks};

    const int num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads{static_cast<size_t>(num_threads)};
    std::this_thread::sleep_for(200ms);

    for (auto _ : state) {
        {
            CONCORE_PROFILING_SCOPE_N("perf iter");
            for (int i = 0; i < num_threads; i++)
                threads[i] = std::thread([i, num_threads, num_tasks, problem_size, &tc]() {
                    CONCORE_PROFILING_SCOPE_N("working");
                    int iterations = num_tasks / num_threads;
                    if (i == 0)
                        iterations += num_tasks % num_threads;
                    for (int k = 0; k < iterations; k++) {
                        CONCORE_PROFILING_SCOPE_N("my task");
                        benchmark::DoNotOptimize(bad_fib(problem_size));
                        tc.task_finished();
                    }
                });
            tc.wait_for_all(10s);
        }
        state.PauseTiming();
        {
            CONCORE_PROFILING_SCOPE_N("iter end");
            // Join the threads
            for (int i = 0; i < num_threads; i++)
                threads[i].join();
            tc.reset(num_tasks);
            std::this_thread::sleep_for(200ms);
        }
    }
}

static void BM_execute_lots_of_tasks_global(benchmark::State& state) {
    CONCORE_PROFILING_SCOPE_N("test global_executor");
    execute_lots_of_tasks(concore::global_executor, state);
}

static void BM_execute_lots_of_tasks_dispatch(benchmark::State& state) {
#ifdef __APPLE__
    CONCORE_PROFILING_SCOPE_N("test dispatch_executor");
    execute_lots_of_tasks(concore::dispatch_executor, state);
#endif
}

static void BM_execute_lots_of_tasks_tbb(benchmark::State& state) {
#ifdef CONCORE_USE_TBB
    CONCORE_PROFILING_SCOPE_N("test tbb_executor");
    tbb::task_scheduler_init init(1+std::thread::hardware_concurrency()); // make it a fair comp
    execute_lots_of_tasks(concore::tbb_executor, state);
#endif
}

static void BM_execute_lots_of_tasks_immediate(benchmark::State& state) {
    CONCORE_PROFILING_SCOPE_N("test immediate_executor");
    execute_lots_of_tasks(concore::immediate_executor, state);
}

#define BENCHMARK_CASE1(fun)                                                                       \
    BENCHMARK(fun)->UseRealTime()->Unit(benchmark::kMillisecond)->Args({1000, 35})

BENCHMARK_CASE1(BM_execute_lots_of_tasks_manual_threads);
BENCHMARK_CASE1(BM_execute_lots_of_tasks_global);
BENCHMARK_CASE1(BM_execute_lots_of_tasks_dispatch);
BENCHMARK_CASE1(BM_execute_lots_of_tasks_tbb);
BENCHMARK_CASE1(BM_execute_lots_of_tasks_manual_threads);
BENCHMARK_CASE1(BM_execute_lots_of_tasks_global);
BENCHMARK_CASE1(BM_execute_lots_of_tasks_dispatch);
BENCHMARK_CASE1(BM_execute_lots_of_tasks_tbb);

#define BENCHMARK_CASE2(fun)                                                                       \
    BENCHMARK(fun)->UseRealTime()->Unit(benchmark::kMillisecond)->Args({1000, 0})

BENCHMARK_CASE2(BM_execute_lots_of_tasks_manual_threads);
BENCHMARK_CASE2(BM_execute_lots_of_tasks_global);
BENCHMARK_CASE2(BM_execute_lots_of_tasks_dispatch);
BENCHMARK_CASE2(BM_execute_lots_of_tasks_tbb);
BENCHMARK_CASE2(BM_execute_lots_of_tasks_immediate);
BENCHMARK_CASE2(BM_execute_lots_of_tasks_manual_threads);
BENCHMARK_CASE2(BM_execute_lots_of_tasks_global);
BENCHMARK_CASE2(BM_execute_lots_of_tasks_dispatch);
BENCHMARK_CASE2(BM_execute_lots_of_tasks_tbb);
BENCHMARK_CASE2(BM_execute_lots_of_tasks_immediate);


BENCHMARK_MAIN();
