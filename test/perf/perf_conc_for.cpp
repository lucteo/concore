
#include <concore/conc_for.hpp>
#include <concore/integral_iterator.hpp>
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
#include <math.h>

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

static void BM_simple_std_for_each(benchmark::State& state) {
    const int data_size = state.range(0);

    std::vector<float> data;
    std::vector<float> out_vec(data_size);
    generate_simple_test_data(data_size, data);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        std::for_each(integral_iterator(0), integral_iterator(data_size),
                [&](int idx) { out_vec[idx] = sin(data[idx]) * cos(data[idx]); });
    }
}

static void BM_simple_conc_for(benchmark::State& state) {
    const int data_size = state.range(0);

    std::vector<float> data;
    std::vector<float> out_vec(data_size);
    generate_simple_test_data(data_size, data);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        concore::conc_for(integral_iterator(0), integral_iterator(data_size),
                [&](int idx) { out_vec[idx] = sin(data[idx]) * cos(data[idx]); });
    }
}

#if CONCORE_USE_TBB
static void BM_simple_tbb_parallel_for(benchmark::State& state) {
    const int data_size = state.range(0);

    std::vector<float> data;
    std::vector<float> out_vec(data_size);
    generate_simple_test_data(data_size, data);

    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        tbb::parallel_for(
                0, data_size, 1, [&](int idx) { out_vec[idx] = sin(data[idx]) * cos(data[idx]); });
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
            out_vec[idx] = sin(data[idx]) * cos(data[idx]);
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
        concore::partition_hints hints;
        hints.granularity_ = data_size / 120;
        concore::conc_for(integral_iterator(0), integral_iterator(data_size),
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

static void BM_____(benchmark::State& /*state*/) {}
#define BENCHMARK_PAUSE() BENCHMARK(BM_____)

#define BENCHMARK_CASE(fun) BENCHMARK(fun)->Unit(benchmark::kMillisecond)->Arg(1000'000);

BENCHMARK_CASE(BM_simple_std_for_each);
BENCHMARK_CASE(BM_simple_conc_for);
#if CONCORE_USE_TBB
BENCHMARK_CASE(BM_simple_tbb_parallel_for);
#endif
#if CONCORE_USE_OPENMP
BENCHMARK_CASE(BM_simple_omp_for);
#endif

#if CONCORE_USE_GLM
BENCHMARK_PAUSE();
BENCHMARK_CASE(BM_fresnel_std_for_each);
BENCHMARK_CASE(BM_fresnel_conc_for);
#if CONCORE_USE_TBB
BENCHMARK_CASE(BM_fresnel_tbb_parallel_for);
#endif
#if CONCORE_USE_OPENMP
BENCHMARK_CASE(BM_fresnel_omp_for);
#endif
#endif // CONCORE_USE_GLM

BENCHMARK_MAIN();
