#include <benchmark/benchmark.h>
#include "flux/util/hash.hpp"
#include <vector>
#include <random>

static void BM_SHA256(benchmark::State& state) {
    // Generate random data
    std::vector<uint8_t> data(state.range(0));
    std::mt19937 gen(42);
    std::uniform_int_distribution<> dis(0, 255);
    for (auto& b : data) b = dis(gen);
    
    for (auto _ : state) {
        auto hash = flux::Hash::compute(flux::HashAlgorithm::SHA256, data);
        benchmark::DoNotOptimize(hash);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)));
}

BENCHMARK(BM_SHA256)->Range(8, 8<<20);

BENCHMARK_MAIN();
