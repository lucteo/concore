
#include <concore/conc_scan.hpp>
#if CONCORE_USE_TBB
#include <tbb/parallel_scan.h>
#include <tbb/blocked_range.h>
#endif

#include <benchmark/benchmark.h>
#include <numeric>

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

static void BM_std_partial_sum(benchmark::State& state) {
    const int data_size = state.range(0);
    std::vector<int> data = generate_test_data(data_size);
    std::vector<int> out(data_size, 0);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        std::partial_sum(data.begin(), data.end(), out.begin(), std::plus<>());
    }
}

static void BM_conc_scan(benchmark::State& state) {
    const int data_size = state.range(0);
    std::vector<int> data = generate_test_data(data_size);
    std::vector<int> out(data_size, 0);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        auto res = concore::conc_scan(
                data.begin(), data.end(), out.begin(), int64_t(0), std::plus<>());
        benchmark::DoNotOptimize(res);
    }
}

#if CONCORE_USE_TBB
template <typename T>
struct scan_body {
    const T* const in_arr;
    T* const out_arr;
    T value{0};
    scan_body(const T* in, T* out)
        : in_arr(in)
        , out_arr(out) {}
    scan_body(scan_body& s, tbb::split)
        : in_arr(s.in_arr)
        , out_arr(s.out_arr) {}
    template <typename Tag>
    void operator()(const tbb::blocked_range<int>& r, Tag) {
        T temp = value;
        for (int i = r.begin(); i < r.end(); ++i) {
            temp = temp + in_arr[i];
            if (Tag::is_final_scan())
                out_arr[i] = temp;
        }
    }
    void reverse_join(scan_body& a) { value = a.value + value; }
    void assign(scan_body& b) { value = b.value; }
};
static void BM_tbb_parallel_scan(benchmark::State& state) {
    const int data_size = state.range(0);
    std::vector<int> data = generate_test_data(data_size);
    std::vector<int> out(data_size, 0);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        scan_body<int> body(&data[0], &out[0]);
        tbb::parallel_scan(tbb::blocked_range(0, data_size), body);
        benchmark::DoNotOptimize(body.value);
    }
}
#endif

static void BM_string_std_partial_sum(benchmark::State& state) {
    const int data_size = state.range(0);
    std::vector<std::string> data = generate_string_test_data(data_size);
    std::vector<std::string> out(data_size, std::string{});

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        std::partial_sum(data.begin(), data.end(), out.begin(), std::plus<>());
    }
}

static void BM_string_conc_scan(benchmark::State& state) {
    const int data_size = state.range(0);
    std::vector<std::string> data = generate_string_test_data(data_size);
    std::vector<std::string> out(data_size, std::string{});

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        auto res = concore::conc_scan(
                data.begin(), data.end(), out.begin(), std::string{}, std::plus<>());
        benchmark::DoNotOptimize(res);
    }
}

#if CONCORE_USE_TBB
static void BM_string_tbb_parallel_scan(benchmark::State& state) {
    const int data_size = state.range(0);
    std::vector<std::string> data = generate_string_test_data(data_size);
    std::vector<std::string> out(data_size, std::string{});

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        scan_body<std::string> body(&data[0], &out[0]);
        tbb::parallel_scan(tbb::blocked_range(0, data_size), body);
        benchmark::DoNotOptimize(body.value);
    }
}
#endif

static void BM_____(benchmark::State& /*state*/) {}
#define BENCHMARK_PAUSE() BENCHMARK(BM_____)

#define BENCHMARK_CASE(fun) BENCHMARK(fun)->Unit(benchmark::kMillisecond)->Arg(10'000'000);
#define BENCHMARK_CASE2(fun) BENCHMARK(fun)->Unit(benchmark::kMillisecond)->Arg(10'000);

BENCHMARK_CASE(BM_std_partial_sum);
BENCHMARK_CASE(BM_conc_scan);
#if CONCORE_USE_TBB
BENCHMARK_CASE(BM_tbb_parallel_scan);
#endif
BENCHMARK_PAUSE();

BENCHMARK_CASE2(BM_string_std_partial_sum);
BENCHMARK_CASE2(BM_string_conc_scan);
#if CONCORE_USE_TBB
BENCHMARK_CASE2(BM_string_tbb_parallel_scan);
#endif

BENCHMARK_MAIN();
