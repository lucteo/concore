#pragma once

#include <benchmark/benchmark.h>

// cppcheck-suppress constParameter
static inline void BM_____(benchmark::State& state) {
    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    for (auto _ : state) {
    }
}
#define BENCHMARK_PAUSE() BENCHMARK(BM_____)
