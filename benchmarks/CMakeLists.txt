include(FetchContent)

FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF)
set(BENCHMARK_DOWNLOAD_DEPENDENCIES ON)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(flux_benchmarks main.cpp)
target_link_libraries(flux_benchmarks PRIVATE benchmark::benchmark flux_core)
