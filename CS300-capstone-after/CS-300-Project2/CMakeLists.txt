cmake_minimum_required(VERSION 3.14)
project(CourseCatalog CXX)

set(CMAKE_CXX_STANDARD 20)          # needed for std::format
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # needed on Windows/MSVC
FetchContent_MakeAvailable(googletest)

add_library(catalog_lib
  src/csv.cpp
  src/helpers.cpp
)

add_executable(app
  src/main.cpp
)

target_link_libraries(app catalog_lib)

enable_testing()

add_executable(csv_test
  tests/csv_test.cpp
)

target_include_directories(app PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_include_directories(catalog_lib PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_include_directories(csv_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/inc")

target_link_libraries(csv_test catalog_lib GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(csv_test)
