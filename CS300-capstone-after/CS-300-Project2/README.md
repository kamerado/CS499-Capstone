# ABCU Course Planner — CS 499 Capstone

[![CI](https://github.com/kamerado/CS499-Capstone/actions/workflows/ci.yml/badge.svg)](https://github.com/kamerado/CS499-Capstone/actions/workflows/ci.yml)

A C++ command-line course planner that loads a university course catalog from a CSV file, stores it in a binary search tree, and lets advisors print the full course list in order or look up a single course and its prerequisites.

The program began as my CS 300 *Data Structures and Algorithms* Project Two. For my CS 499 Computer Science Capstone, I am enhancing it in three areas: **software design and engineering**, **algorithms and data structures**, and **databases**. Each enhancement fixes weaknesses found in a formal code review of the original.

## Enhancement status

| Category | Focus | Status |
|---|---|---|
| Software design & engineering | Modular structure, memory safety, input hardening, tests, CI | ✅ Complete |
| Algorithms & data structures | Self-balancing AVL tree, prerequisite graph (DFS, cycle detection, topological sort), benchmarks | 🚧 In progress |
| Databases | Normalized SQLite storage with parameterized queries | 📋 Planned |

### Software design & engineering highlights

- **Modular layout.** The original single file is split into focused modules (`course`, `bst`, `csv`, `helpers`), with dependencies flowing one way and no circular includes.
- **Memory safety.** Raw `new`/`delete` replaced with `std::unique_ptr`. The tree is non-copyable and movable, so the original shared-node double free is now a compile error. Verified clean under AddressSanitizer, UndefinedBehaviorSanitizer and Valgrind.
- **Bug fixes.** Removing the root no longer leaves a dangling pointer, removing a node with two children no longer corrupts the tree, duplicate courses are rejected, and printing before loading no longer crashes.
- **Input hardening.** Menu input rejects non-numeric, partially numeric (`123abc`) and out-of-range values and handles end of input. The CSV parser trims whitespace and Windows `\r\n` line endings, tolerates trailing commas, and validates every row.
- **Clear error reporting.** Malformed CSV files raise typed exceptions with line numbers or course IDs instead of exiting the program.
- **Tooling.** CMake build, GoogleTest unit tests, Google Benchmark performance tests, and GitHub Actions CI that runs the test suite in both a normal and a sanitizer build on every push.

## Repository layout

```
CS300            original CS 300 code, unchanged (for comparison)
CS300-capstone   CS 300 code with comments for planning improvements and code review.
CS300-capstone-after/CS-300-Project2/
├── CMakeLists.txt
├── inc/          headers (course.h, bst.h, csv.h, helpers.h)
├── src/          sources and main.cpp
├── tests/        GoogleTest unit tests
└── bench/        Google Benchmark performance tests
.github/workflows/ci.yml   build, test, and sanitizer jobs
```

## Building

**Requirements**

- CMake 3.14 or newer
- A C++20 compiler (GCC 13+, Clang 17+, or a recent MSVC)
- Internet access on the first configure (GoogleTest and Google Benchmark are downloaded automatically)

```bash
cd CS300-capstone-after/CS-300-Project2
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Usage

```bash
./build/app
```

| Option | Action |
|---|---|
| `1` | Load a course catalog from a CSV file |
| `2` | Print all courses in alphanumeric order |
| `3` | Print one course with its prerequisites |
| `9` | Exit |

### CSV format

Each line is a course ID, a title, and zero or more prerequisite course IDs:

```
CSCI100,Introduction to Computer Science
CSCI200,Data Structures,CSCI100
CSCI300,Introduction to Algorithms,CSCI200,MATH201
MATH201,Discrete Mathematics
```

A file is rejected, with a message naming the problem, if any line is missing an ID or title, a course lists itself as a prerequisite, a course ID appears twice, or a prerequisite is not a course in the file. Blank lines, surrounding whitespace, trailing commas, and Windows line endings are handled automatically.

## Testing

```bash
ctest --test-dir build --output-on-failure
```

### Sanitizer build

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

### Valgrind

```bash
valgrind --leak-check=full --show-leak-kinds=all ./build/tests
```

## Benchmarks

Benchmarks compare tree performance on sorted and shuffled course IDs. Sorted input is the plain BST's worst case, and it is how real catalogs usually arrive.

```bash
./build/tree_bench --benchmark_repetitions=5 --benchmark_report_aggregates_only=true
```

Always benchmark a Release build. Results and the BST-vs-AVL comparison will be added here once the algorithms enhancement is complete.

## About

Created by [kamerado] for SNHU CS 499, Computer Science Capstone. Original artifact: CS 300 Project Two.
