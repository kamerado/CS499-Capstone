cmake_minimum_required(VERSION 3.14)
project(CourseCatalog C CXX)          # C for the SQLite amalgamation

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

# SQLite: built from the official single-file "amalgamation" (sqlite3.c +
# sqlite3.h, public domain), downloaded like GoogleTest. Nothing has to be
# installed, and it builds the same way on Linux, macOS and Windows.
# The hash is SQLite's published SHA3-256, so a corrupted or tampered download
# fails the configure step instead of being compiled.
# To use an installed SQLite instead: cmake -DUSE_SYSTEM_SQLITE=ON ...
option(USE_SYSTEM_SQLITE "Link the system SQLite instead of building it" OFF)

if(USE_SYSTEM_SQLITE)
  find_package(SQLite3 REQUIRED)
else()
  FetchContent_Declare(
    sqlite3
    # SQLite files releases under a year folder; both are tried in order.
    URL https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
        https://www.sqlite.org/2025/sqlite-amalgamation-3540100.zip
    URL_HASH SHA3_256=92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30
  )
  FetchContent_MakeAvailable(sqlite3) # the zip has no CMakeLists; just unpacks

  add_library(sqlite3 STATIC "${sqlite3_SOURCE_DIR}/sqlite3.c")
  target_include_directories(sqlite3 SYSTEM PUBLIC "${sqlite3_SOURCE_DIR}")
  target_compile_definitions(sqlite3 PRIVATE
    SQLITE_DQS=0                  # "text" is an identifier, never a string
    SQLITE_OMIT_LOAD_EXTENSION    # no run-time loading of native code
  )
  # Third-party code: don't flood the build with its warnings.
  if(MSVC)
    target_compile_options(sqlite3 PRIVATE /W0)
  else()
    target_compile_options(sqlite3 PRIVATE -w)
  endif()
  find_package(Threads REQUIRED)  # SQLite is built thread-safe
  target_link_libraries(sqlite3 PUBLIC Threads::Threads)
  if(UNIX)
    target_link_libraries(sqlite3 PUBLIC m)
  endif()
  # Same name FindSQLite3 uses, so the rest of the file doesn't care which.
  add_library(SQLite::SQLite3 ALIAS sqlite3)
endif()

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
  src/graph.cpp
  src/course_service.cpp
  src/csv_repository.cpp
  src/sqlite_repository.cpp
)
target_include_directories(catalog_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_link_libraries(catalog_lib PUBLIC SQLite::SQLite3)

add_executable(app src/main.cpp)
target_link_libraries(app PRIVATE catalog_lib)
# Default database location: the build folder, next to the other build output
# (and already ignored by .gitignore). A path typed at the prompt or given on
# the command line still wins.
target_compile_definitions(app PRIVATE
  CATALOG_DEFAULT_DB="${CMAKE_BINARY_DIR}/catalog.db")

add_executable(tree_bench bench/tree_bench.cpp)
target_link_libraries(tree_bench PRIVATE catalog_lib benchmark::benchmark)

# ---- Tests ----------------------------------------------------------------
enable_testing()
include(GoogleTest)

function(add_unit_test name)
  add_executable(${name}_test tests/${name}_test.cpp)
  target_link_libraries(${name}_test PRIVATE catalog_lib GTest::gtest_main)
  target_compile_definitions(${name}_test PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  gtest_discover_tests(${name}_test
    TEST_PREFIX "${name}/"
    PROPERTIES LABELS "${name}")
endfunction()

add_unit_test(csv)
add_unit_test(bst)
add_unit_test(avl)
add_unit_test(graph)
add_unit_test(repository)
add_unit_test(service)
