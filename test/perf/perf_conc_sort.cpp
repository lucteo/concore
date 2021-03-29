
#include <concore/conc_sort.hpp>
#include <concore/profiling.hpp>
#if CONCORE_USE_TBB
#include <tbb/parallel_sort.h>
#endif

#include <benchmark/benchmark.h>
#include <algorithm>
#include <chrono>

//! Produces a integer in range [-100, 100]
int rand_small_int() { return rand() % (200) - 100; }

char rand_char() {
    static constexpr int num = 'z' - 'a';
    return static_cast<char>('a' + rand() % num);
}

std::string rand_string() {
    int size = rand() % 100;
    std::string res(size, 'a');
    std::generate(res.begin(), res.end(), &rand_char);
    return res;
}

std::vector<int> generate_test_data(int size) {
    srand(0); // Same values each time
    std::vector<int> res(size, 0);
    std::generate(res.begin(), res.end(), &rand_small_int);
    return res;
}

std::vector<std::string> generate_string_test_data(int size) {
    srand(0); // Same values each time
    std::vector<std::string> res(size, std::string{});
    std::generate(res.begin(), res.end(), &rand_string);
    return res;
}

static void BM_std_sort(benchmark::State& state) {
    const int data_size = state.range(0);
    std::vector<int> test_data = generate_test_data(data_size);
    std::vector<int> data;

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        // Copy the data
        data = test_data;

        // Sort and measure the time
        auto start = std::chrono::high_resolution_clock::now();

        std::sort(data.begin(), data.end());

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_sec = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_sec.count());
    }
}

static void BM_conc_sort(benchmark::State& state) {
    const int data_size = state.range(0);
    std::vector<int> test_data = generate_test_data(data_size);
    std::vector<int> data;

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        // Copy the data
        data = test_data;

        // Sort and measure the time
        auto start = std::chrono::high_resolution_clock::now();

        concore::conc_sort(data.begin(), data.end());

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_sec = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_sec.count());
    }
}

#if CONCORE_USE_TBB
static void BM_tbb_parallel_sort(benchmark::State& state) {
    const int data_size = state.range(0);
    std::vector<int> test_data = generate_test_data(data_size);
    std::vector<int> data(data_size, 0);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        // Copy the data
        data = test_data;

        // Sort and measure the time
        auto start = std::chrono::high_resolution_clock::now();

        tbb::parallel_sort(data.begin(), data.end());

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_sec = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_sec.count());
    }
}
#endif

static void BM_string_std_sort(benchmark::State& state) {
    const int data_size = state.range(0);
    std::vector<std::string> test_data = generate_string_test_data(data_size);
    std::vector<std::string> data;

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        // Copy the data
        data = test_data;

        // Sort and measure the time
        auto start = std::chrono::high_resolution_clock::now();

        std::sort(data.begin(), data.end());

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_sec = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_sec.count());
    }
}

static void BM_string_conc_sort(benchmark::State& state) {
    const int data_size = state.range(0);
    std::vector<std::string> test_data = generate_string_test_data(data_size);
    std::vector<std::string> data;

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        // Copy the data
        data = test_data;

        // Sort and measure the time
        auto start = std::chrono::high_resolution_clock::now();

        concore::conc_sort(data.begin(), data.end());

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_sec = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_sec.count());
    }
}

#if CONCORE_USE_TBB
static void BM_string_tbb_parallel_sort(benchmark::State& state) {
    const int data_size = state.range(0);
    std::vector<std::string> test_data = generate_string_test_data(data_size);
    std::vector<std::string> data(data_size, std::string{});

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        // Copy the data
        data = test_data;

        // Sort and measure the time
        auto start = std::chrono::high_resolution_clock::now();

        tbb::parallel_sort(data.begin(), data.end());

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_sec = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_sec.count());
    }
}
#endif

static void BM_____(benchmark::State& /*state*/) {}
#define BENCHMARK_PAUSE() BENCHMARK(BM_____)

#define BENCHMARK_CASE(fun)                                                                        \
    BENCHMARK(fun)->UseManualTime()->Unit(benchmark::kMillisecond)->Arg(10'000'000);
#define BENCHMARK_CASE2(fun)                                                                       \
    BENCHMARK(fun)->UseManualTime()->Unit(benchmark::kMillisecond)->Arg(50'000);

BENCHMARK_CASE(BM_std_sort);
BENCHMARK_CASE(BM_conc_sort);
#if CONCORE_USE_TBB
BENCHMARK_CASE(BM_tbb_parallel_sort);
#endif
BENCHMARK_PAUSE();

BENCHMARK_CASE2(BM_string_std_sort);
BENCHMARK_CASE2(BM_string_conc_sort);
#if CONCORE_USE_TBB
BENCHMARK_CASE2(BM_string_tbb_parallel_sort);
#endif

BENCHMARK_MAIN();
