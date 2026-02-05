cmake_minimum_required(VERSION 3.20)
project(FluxVCS VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(FLUX_BUILD_TESTS "Build tests" ON)
option(FLUX_BUILD_BENCHMARKS "Build benchmarks" OFF)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-deprecated-declarations)
endif()

# Dependencies
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)

# Third-party libraries
# Try to find system packages first (required for Debian/Ubuntu packaging)
# Fall back to FetchContent if not available

find_package(CLI11 QUIET)
find_package(fmt QUIET)

if(NOT CLI11_FOUND OR NOT fmt_FOUND)
    include(FetchContent)
    
    if(NOT CLI11_FOUND)
        # CLI11 for argument parsing
        FetchContent_Declare(
            cli11
            GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
            GIT_TAG v2.3.2
        )
        FetchContent_MakeAvailable(cli11)
    endif()
    
    if(NOT fmt_FOUND)
        # {fmt} for formatting
        FetchContent_Declare(
            fmt
            GIT_REPOSITORY https://github.com/fmtlib/fmt.git
            GIT_TAG 10.1.1
        )
        FetchContent_MakeAvailable(fmt)
    endif()
endif()

# Core library
add_library(flux_core STATIC
    src/core/types.cpp
    src/core/repository.cpp
    src/core/object_id.cpp
    src/core/auth.cpp
    src/core/commit.cpp
    src/core/tree.cpp
    src/core/blob.cpp
    src/storage/object_store.cpp
    src/storage/ref_store.cpp
    src/storage/wal.cpp
    src/storage/snapshot.cpp
    src/storage/packfile.cpp
    src/index/index.cpp
    src/verify/integrity.cpp
    src/merge/simple_parser.cpp
    src/merge/semantic_diff.cpp
    src/merge/merger.cpp
    src/cache/object_cache.cpp
    src/security/gpg.cpp
    src/net/git_protocol.cpp
    src/net/http_transport.cpp
    src/net/fetch.cpp
    src/net/git_pack.cpp
    src/net/git_pack_packer.cpp
    src/net/push.cpp
    src/util/chunker.cpp
    src/util/hash.cpp
    src/util/compression.cpp
    src/util/zlib_util.cpp
    src/util/filesystem.cpp
)

target_include_directories(flux_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(flux_core PUBLIC 
    CLI11::CLI11
    fmt::fmt
    SQLite::SQLite3
    PkgConfig::ZSTD
    ${ZSTD_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    CURL::libcurl
    ZLIB::ZLIB
)
add_executable(flux
    src/cli/main.cpp
    src/cli/commands/init.cpp
    src/cli/commands/add.cpp
    src/cli/commands/commit.cpp
    src/cli/commands/branch.cpp
    src/cli/commands/log.cpp
    src/cli/commands/status.cpp
    src/cli/commands/fsck.cpp
    src/cli/commands/merge.cpp
    src/cli/commands/gc.cpp
    src/cli/commands/gpg.cpp
    src/cli/commands/network.cpp
    src/cli/commands/checkout.cpp
    src/cli/commands/config.cpp
)

target_link_libraries(flux PRIVATE 
    flux_core
    CLI11::CLI11
)

# Benchmarks
if(FLUX_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Tests
if(FLUX_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Installation rules
include(GNUInstallDirs)
install(TARGETS flux DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS flux_core DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/flux DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# CPack packaging
set(CPACK_PACKAGE_NAME "flux-vcs")
set(CPACK_PACKAGE_VENDOR "FluxVCS Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Next-generation version control system")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Ignore build directory, git metadata, debian directory (generated separately), and test artifacts
set(CPACK_SOURCE_IGNORE_FILES
    "/.git/"
    "/.flux/"
    "/build/"
    "/debian/"
    "/.*\\\\.DS_Store"
    "/.*\\\\.swp"
    "/.*_repo/"
    "/.*_test/"
    "/local_.*/"
    "/upstream_.*/"
    "/downstream_.*/"
    "/clone.log"
    "/DevWire.*/"
    "/hello/"
)

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Grady Korchinski <grady.korchinski@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsqlite3-0, libssl3, libcurl4, zlib1g, libzstd1")

include(CPack)
