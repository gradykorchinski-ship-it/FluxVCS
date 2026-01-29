FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

FetchContent_MakeAvailable(googletest)

enable_testing()

# Core tests
add_executable(core_tests
    core/test_object_id.cpp
    core/test_chunker.cpp
)

target_link_libraries(core_tests PRIVATE
    flux_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(core_tests)
