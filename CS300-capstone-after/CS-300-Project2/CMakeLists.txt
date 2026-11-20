cmake_minimum_required(VERSION 3.14)
project(CourseCatalog CXX)

set(CMAKE_CXX_STANDARD 20)          # needed for std::format
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Dependencies ---------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # needed on Windows/MSVC
FetchContent_MakeAvailable(googletest)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  benchmark
  URL https://github.com/google/benchmark/archive/refs/tags/v1.9.0.zip
)
FetchContent_MakeAvailable(benchmark)

# ---- Warnings for our own code only --------------------------------------
# Set after the dependencies, so GoogleTest and Benchmark aren't affected.
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- Project code ---------------------------------------------------------
add_library(catalog_lib
  src/csv.cpp
  src/helpers.cpp
)
target_include_directories(catalog_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")

add_executable(app src/main.cpp)
target_link_libraries(app PRIVATE catalog_lib)

add_executable(tree_bench bench/tree_bench.cpp)
target_link_libraries(tree_bench PRIVATE catalog_lib benchmark::benchmark)

# ---- Tests ----------------------------------------------------------------
enable_testing()
include(GoogleTest)

function(add_unit_test name)
  add_executable(${name}_test tests/${name}_test.cpp)
  target_link_libraries(${name}_test PRIVATE catalog_lib GTest::gtest_main)
  gtest_discover_tests(${name}_test
    TEST_PREFIX "${name}/"
    PROPERTIES LABELS "${name}")
endfunction()

add_unit_test(csv)
add_unit_test(bst)
add_unit_test(avl)
