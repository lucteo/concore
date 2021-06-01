
#include "benchmark_helpers.hpp"
#include <concore/conc_for.hpp>
#include <concore/integral_iterator.hpp>
#include <concore/profiling.hpp>
#if CONCORE_USE_TBB
#include <tbb/parallel_for.h>
#endif

#if CONCORE_USE_OPENMP
#include <omp.h>
#endif

#if CONCORE_USE_GLM
#include <glm/vec4.hpp>
#include <glm/geometric.hpp>
#endif

#include <benchmark/benchmark.h>
#include <algorithm>
#include <cmath>

using concore::integral_iterator;

//! Produces a random float in range [-1, 1]
float rand_float() {
    static constexpr int resolution = 10000;
    return static_cast<float>(rand() % (2 * resolution) - resolution) /
           static_cast<float>(resolution);
}

void generate_simple_test_data(int size, std::vector<float>& data) {
    srand(0); // Same values each time
    data.clear();
    data.resize(size_t(size), 0.0f);
    std::generate(data.begin(), data.end(), [] { return rand_float() * 3.14159f; });
}

float simple_transform(float in) {
    return std::sqrt(std::sin(in)) * std::sqrt(std::cos(in)) +
           std::sqrt(std::sin(2 * in)) * std::sqrt(std::cos(2 * in)) +
           std::sqrt(std::sin(3 * in)) * std::sqrt(std::cos(3 * in)) +
           std::sqrt(std::sin(4 * in)) * std::sqrt(std::cos(4 * in));
}

static void BM_simple_std_for_each(benchmark::State& state) {
    const int data_size = state.range(0);

    std::vector<float> data;
    std::vector<float> out_vec(data_size);
    generate_simple_test_data(data_size, data);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        std::for_each(integral_iterator(0), integral_iterator(data_size),
                [&](int idx) { out_vec[idx] = simple_transform(data[idx]); });
    }
}

struct simple_work {
    using iterator = int;

    const float* data_;
    float* out_vec_;

    simple_work(const std::vector<float>& data, std::vector<float>& out_vec)
        : data_(&data[0])
        , out_vec_(&out_vec[0]) {}

    void exec(int first, int last) {
        CONCORE_PROFILING_SCOPE_N("body work");
        for (; first != last; first++)
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            out_vec_[first] = simple_transform(data_[first]);
    }
};

static void BM_simple_conc_for(benchmark::State& state) {
    const int data_size = state.range(0);

    std::vector<float> data;
    std::vector<float> out_vec(data_size);
    generate_simple_test_data(data_size, data);
    simple_work w(data, out_vec);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        concore::conc_for(0, data_size, w);
    }
}

static void BM_simple_conc_for_upfront(benchmark::State& state) {
    const int data_size = state.range(0);

    std::vector<float> data;
    std::vector<float> out_vec(data_size);
    generate_simple_test_data(data_size, data);
    simple_work w(data, out_vec);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::upfront_partition;
        hints.tasks_per_worker_ = 10;
        concore::conc_for(0, data_size, w, hints);
    }
}

static void BM_simple_conc_for_iterative(benchmark::State& state) {
    const int data_size = state.range(0);

    std::vector<float> data;
    std::vector<float> out_vec(data_size);
    generate_simple_test_data(data_size, data);
    simple_work w(data, out_vec);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::iterative_partition;
        concore::conc_for(0, data_size, w, hints);
    }
}

static void BM_simple_conc_for_it(benchmark::State& state) {
    const int data_size = state.range(0);

    std::vector<float> data;
    std::vector<float> out_vec(data_size);
    generate_simple_test_data(data_size, data);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        concore::conc_for(
                0, data_size, [&](int idx) { out_vec[idx] = simple_transform(data[idx]); });
    }
}

static void BM_simple_conc_for_upfront_it(benchmark::State& state) {
    const int data_size = state.range(0);

    std::vector<float> data;
    std::vector<float> out_vec(data_size);
    generate_simple_test_data(data_size, data);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::upfront_partition;
        hints.tasks_per_worker_ = 10;
        concore::conc_for(
                0, data_size, [&](int idx) { out_vec[idx] = simple_transform(data[idx]); }, hints);
    }
}

static void BM_simple_conc_for_iterative_it(benchmark::State& state) {
    const int data_size = state.range(0);

    std::vector<float> data;
    std::vector<float> out_vec(data_size);
    generate_simple_test_data(data_size, data);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::iterative_partition;
        concore::conc_for(
                0, data_size, [&](int idx) { out_vec[idx] = simple_transform(data[idx]); }, hints);
    }
}

#if CONCORE_USE_TBB
struct simple_body {
    const std::vector<float>& data_;
    std::vector<float>& out_vec_;

    simple_body(const std::vector<float>& data, std::vector<float>& out_vec)
        : data_(data)
        , out_vec_(out_vec) {}

    void operator()(const tbb::blocked_range<int>& r) const {
        CONCORE_PROFILING_SCOPE_N("body work");
        for (int i = r.begin(); i != r.end(); i++)
            out_vec_[i] = simple_transform(data_[i]);
    }
};

static void BM_simple_tbb_parallel_for(benchmark::State& state) {
    const int data_size = state.range(0);

    std::vector<float> data;
    std::vector<float> out_vec(data_size);
    generate_simple_test_data(data_size, data);
    simple_body body(data, out_vec);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        tbb::parallel_for(tbb::blocked_range<int>(0, data_size), body);
    }
}
static void BM_simple_tbb_parallel_for_it(benchmark::State& state) {
    const int data_size = state.range(0);

    std::vector<float> data;
    std::vector<float> out_vec(data_size);
    generate_simple_test_data(data_size, data);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        tbb::parallel_for(
                0, data_size, 1, [&](int idx) { out_vec[idx] = simple_transform(data[idx]); });
    }
}
#endif

#if CONCORE_USE_OPENMP
static void BM_simple_omp_for(benchmark::State& state) {
    const int data_size = state.range(0);

    std::vector<float> data;
    std::vector<float> out_vec(data_size);
    generate_simple_test_data(data_size, data);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
#pragma omp parallel for
        for (int idx = 0; idx < data_size; idx++) {
            out_vec[idx] = simple_transform(data[idx]);
        };
    }
}
#endif

#if CONCORE_USE_GLM
// similar with Bartek's banchmark: https://www.bfilipek.com/2018/11/parallel-alg-perf.html
float fresnel(const glm::vec4& I, const glm::vec4& N, float ior) {
    float cosi = std::clamp(glm::dot(I, N), -1.0f, 1.0f);
    float etai = 1, etat = ior;
    if (cosi > 0) {
        std::swap(etai, etat);
    }
    // Compute sini using Snell's law
    float sint = etai / etat * sqrtf(std::max(0.f, 1 - cosi * cosi));
    // Total internal reflection
    if (sint >= 1)
        return 1.0f;

    float cost = sqrtf(std::max(0.f, 1 - sint * sint));
    cosi = fabsf(cosi);
    float Rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost));
    float Rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost));
    return (Rs * Rs + Rp * Rp) / 2.0f;
}

glm::vec4 rand_vec4() { return glm::vec4(rand_float(), rand_float(), rand_float(), 1.0f); }

using vec_array = std::vector<glm::vec4>;

void generate_fresnel_test_data(int size, vec_array& incidence, vec_array& normals) {
    srand(0); // Same values each time
    incidence.clear();
    normals.clear();
    incidence.resize(size_t(size), glm::vec4{});
    normals.resize(size_t(size), glm::vec4{});
    std::generate(incidence.begin(), incidence.end(), &rand_vec4);
    std::generate(normals.begin(), normals.end(), &rand_vec4);
}

static void BM_fresnel_std_for_each(benchmark::State& state) {
    const int data_size = state.range(0);

    vec_array incidence, normals;
    std::vector<float> out_vec(data_size);
    generate_fresnel_test_data(data_size, incidence, normals);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        std::for_each(integral_iterator(0), integral_iterator(data_size),
                [&](int idx) { out_vec[idx] = fresnel(incidence[idx], normals[idx], 1.0f); });
    }
}

static void BM_fresnel_conc_for(benchmark::State& state) {
    const int data_size = state.range(0);

    vec_array incidence, normals;
    std::vector<float> out_vec(data_size);
    generate_fresnel_test_data(data_size, incidence, normals);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        concore::conc_for(0, data_size,
                [&](int idx) { out_vec[idx] = fresnel(incidence[idx], normals[idx], 1.0f); });
    }
}

static void BM_fresnel_conc_for_upfront(benchmark::State& state) {
    const int data_size = state.range(0);

    vec_array incidence, normals;
    std::vector<float> out_vec(data_size);
    generate_fresnel_test_data(data_size, incidence, normals);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        concore::partition_hints hints;
        hints.method_ = concore::partition_method::upfront_partition;
        hints.tasks_per_worker_ = 10;
        concore::conc_for(
                0, data_size,
                [&](int idx) { out_vec[idx] = fresnel(incidence[idx], normals[idx], 1.0f); },
                hints);
    }
}

#if CONCORE_USE_TBB
static void BM_fresnel_tbb_parallel_for(benchmark::State& state) {
    const int data_size = state.range(0);

    vec_array incidence, normals;
    std::vector<float> out_vec(data_size);
    generate_fresnel_test_data(data_size, incidence, normals);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        tbb::parallel_for(0, data_size, 1,
                [&](int idx) { out_vec[idx] = fresnel(incidence[idx], normals[idx], 1.0f); });
    }
}
#endif

#if CONCORE_USE_OPENMP
static void BM_fresnel_omp_for(benchmark::State& state) {
    const int data_size = state.range(0);

    vec_array incidence, normals;
    std::vector<float> out_vec(data_size);
    generate_fresnel_test_data(data_size, incidence, normals);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
#pragma omp parallel for
        for (int idx = 0; idx < data_size; idx++) {
            out_vec[idx] = fresnel(incidence[idx], normals[idx], 1.0f);
        };
    }
}
#endif
#endif // CONCORE_USE_GLM

#define BENCHMARK_CASE(fun) BENCHMARK(fun)->Unit(benchmark::kMillisecond)->Arg(1'000'000);

BENCHMARK_CASE(BM_simple_std_for_each);
BENCHMARK_CASE(BM_simple_conc_for);
BENCHMARK_CASE(BM_simple_conc_for_upfront);
BENCHMARK_CASE(BM_simple_conc_for_iterative);
BENCHMARK_CASE(BM_simple_conc_for_it);
BENCHMARK_CASE(BM_simple_conc_for_upfront_it);
BENCHMARK_CASE(BM_simple_conc_for_iterative_it);
#if CONCORE_USE_TBB
BENCHMARK_CASE(BM_simple_tbb_parallel_for);
BENCHMARK_CASE(BM_simple_tbb_parallel_for_it);
#endif
#if CONCORE_USE_OPENMP
BENCHMARK_CASE(BM_simple_omp_for);
#endif

#if CONCORE_USE_GLM
BENCHMARK_PAUSE();
BENCHMARK_CASE(BM_fresnel_std_for_each);
BENCHMARK_CASE(BM_fresnel_conc_for);
BENCHMARK_CASE(BM_fresnel_conc_for_upfront);
#if CONCORE_USE_TBB
BENCHMARK_CASE(BM_fresnel_tbb_parallel_for);
#endif
#if CONCORE_USE_OPENMP
BENCHMARK_CASE(BM_fresnel_omp_for);
#endif
#endif // CONCORE_USE_GLM

BENCHMARK_MAIN();
