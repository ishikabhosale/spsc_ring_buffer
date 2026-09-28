cmake_minimum_required(VERSION 3.20)
project(spsc_ring_buffer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(spsc_ring_buffer INTERFACE)
add_library(spsc::ring_buffer ALIAS spsc_ring_buffer)
target_include_directories(spsc_ring_buffer INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_compile_features(spsc_ring_buffer INTERFACE cxx_std_17)

add_executable(spsc_demo main.cpp)
target_link_libraries(spsc_demo PRIVATE spsc::ring_buffer)

add_executable(spsc_thread_test thread_safety_test.cpp)
target_link_libraries(spsc_thread_test PRIVATE spsc::ring_buffer Threads::Threads)

add_executable(spsc_unit_test spsc_unit_test.cpp)
target_link_libraries(spsc_unit_test PRIVATE spsc::ring_buffer)

enable_testing()
add_test(NAME spsc_thread_safety COMMAND spsc_thread_test)
add_test(NAME spsc_unit COMMAND spsc_unit_test)

option(SPSC_BUILD_BENCHMARKS "Build the Google Benchmark suite" ON)
if(SPSC_BUILD_BENCHMARKS)
    include(FetchContent)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(spsc_bench spsc_bench.cpp)
    target_link_libraries(spsc_bench PRIVATE spsc::ring_buffer benchmark::benchmark Threads::Threads)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(spsc_bench PRIVATE -O2 -march=native -Wall -Wextra -Wpedantic)
    endif()
endif()
