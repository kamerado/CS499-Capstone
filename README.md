# ABCU Course Planner — CS 499 Capstone

[![CI](https://github.com/kamerado/CS499-Capstone/actions/workflows/ci.yml/badge.svg)](https://github.com/kamerado/CS499-Capstone/actions/workflows/ci.yml)

A C++ command-line course planner for ABCU advisors. It keeps a university course catalog in a SQLite database (or reads it from a CSV file), caches it in a self-balancing search tree and a prerequisite graph, and lets advisors look up courses, plan terms, track completed courses, and add, edit or remove courses.

The program began as my CS 300 *Data Structures and Algorithms* Project Two. For my CS 499 Computer Science Capstone, I am enhancing it in three areas: **software design and engineering**, **algorithms and data structures**, and **databases**. Each enhancement fixes weaknesses found in a formal code review of the original.

## Enhancement status

| Category | Focus | Status |
|---|---|---|
| Software design & engineering | Modular structure, memory safety, input hardening, tests, CI | ✅ Complete |
| Algorithms & data structures | Self-balancing AVL tree, prerequisite graph (DFS, cycle detection, topological sort), benchmarks | 🚧 In progress |
| Databases | Storage interface, normalized SQLite storage, parameterized queries, CRUD | 🚧 In progress |

## Architecture

```
            CLI (src/main.cpp)              presentation: menus and console I/O
                    |
              CourseService                  business logic: search, validation,
                    |                        cycle detection, eligibility
                    v
           ICourseRepository                 storage interface (abstract base class)
              ^           ^
              :           :
     CsvRepository    SqliteRepository        backends
     (read-only)      (source of truth)
```

- **The CLI and the service only know `ICourseRepository`.** `main()` is the one place that picks a concrete backend; after that, everything goes through the interface. Adding another backend (a REST API, say) means writing one class, with no changes to the service or the menu. This is the dependency-inversion principle.
- **Storage stores, the service decides.** Repositories save and return courses and enforce simple integrity (no duplicates, no unknown prerequisites). `CourseService` validates the whole catalog, including circular prerequisites, which no database constraint can express, before anything is written.
- **Storage first, then cache.** The AVL tree (or BST) and the prerequisite graph are an in-memory cache built from the repository. A change is validated, written to storage, and only then applied to the cache, so a failed write never leaves the two out of step.
- **One contract, many implementations.** The repository contract tests are written only against the interface and run against every backend (SQLite in memory, SQLite on disk, and an in-memory test fake).

### Databases highlights

- **Normalized schema** with `courses`, `prerequisites` and `completed_courses` tables. Primary keys, foreign keys, `NOT NULL` and `CHECK` constraints mean the database itself rejects duplicate courses, unknown prerequisites and self-prerequisites, even if a caller has a bug. Foreign keys are switched on for every connection and verified.
- **Parameterized queries only.** Every query is a prepared statement with bound parameters; user input is never concatenated into SQL. A test stores an SQL-injection string as a course ID and checks it round-trips as plain text.
- **Transactions everywhere.** Every write, including a full CSV import, runs in a transaction that rolls back on any error, so a failure leaves the database exactly as it was.
- **RAII for SQLite handles.** Connections, statements and transactions are wrapped in C++ objects that clean up on every path, including exceptions. Verified under AddressSanitizer and UndefinedBehaviorSanitizer.
- **Schema versioning** with `PRAGMA user_version`, so a database created by a newer version is refused instead of misread.
- **New features:** add, update and remove courses; mark courses completed; "courses I can take next", answered both in memory and in pure SQL, with a test that walks a whole degree and checks both answers agree at every step.

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
├── inc/          headers
│   ├── course.h              Course model shared by every layer
│   ├── course_repository.h   ICourseRepository interface and its errors
│   ├── csv_repository.h      read-only CSV backend
│   ├── sqlite_repository.h   SQLite backend
│   ├── course_service.h      business logic and in-memory cache
│   └── avl.h, bst.h, graph.h, csv.h, helpers.h, Treeprint.h
├── src/          sources and main.cpp (CLI + composition root)
├── tests/        GoogleTest unit tests
└── bench/        Google Benchmark performance tests
.github/workflows/ci.yml   build, test, and sanitizer jobs
```

## Building

**Requirements**

- CMake 3.14 or newer
- A C++20 compiler (GCC 13+, Clang 17+, or a recent MSVC)
- Internet access on the first configure (GoogleTest, Google Benchmark and SQLite are downloaded automatically)

No libraries need to be installed. SQLite is compiled from its official single-file amalgamation, checked against SQLite's published SHA3-256 hash. To use an installed SQLite instead, configure with `-DUSE_SYSTEM_SQLITE=ON`.

```bash
cd CS300-capstone-after/CS-300-Project2
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Usage

```bash
./build/app                              # choose a CSV file or a database at startup
./build/app CS300_ABCU_Input.csv         # open a CSV file (read-only)
./build/app catalog.db                   # open or create a SQLite database
```

Choosing the database option and pressing Enter uses `catalog.db` in the build folder, which `.gitignore` already excludes. A new, empty database offers to import a CSV file straight away. Files ending in `.db`, `.sqlite` or `.sqlite3` are opened as databases.

| Option | Action |
|---|---|
| `1` | Print all courses in alphanumeric order |
| `2` | Print the search tree |
| `3` | Print one course with its prerequisites |
| `4` | Save the search tree as an SVG image |
| `5` | Print a term-by-term course plan |
| `6` | Print every prerequisite of a course, direct and indirect |
| `7` | Courses I can take next (prerequisites all completed) |
| `8` | Switch between the AVL tree and the BST |
| `10` | Add a course *(database only)* |
| `11` | Update a course's title or prerequisites *(database only)* |
| `12` | Remove a course *(database only)* |
| `13` | Mark a course completed or not completed *(database only)* |
| `14` | Import a CSV file, replacing the catalog *(database only)* |
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
