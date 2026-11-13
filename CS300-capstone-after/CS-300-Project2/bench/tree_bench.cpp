#include "bst.h"
#include "course.h"
#include <algorithm>
#include <benchmark/benchmark.h>
#include <format>
#include <random>

// This is a function that generates a large number of dummy courseIDS.
static std::vector<Course> makeCourses(int n, bool shuffled) {
  std::vector<Course> v;
  v.reserve(n);
  for (int i = 0; i < n; i++) {
    // Zero-padded so string order matches numeric order: CS000009 < CS000010.
    v.emplace_back(std::format("CS{:06}", i), "Title");
  }
  if (shuffled) {
    std::mt19937 rng(42); // fixed seed: same "random" order every run
    std::shuffle(v.begin(), v.end(), rng);
  }
  return v;
}

template <typename Tree, bool Shuffled>
static void BM_Insert(benchmark::State &state) {
  auto courses = makeCourses(state.range(0), Shuffled);
  for (auto _ : state) {
    Tree tree;
    for (const auto &c : courses)
      tree.Insert(c);
  }
  state.SetComplexityN(state.range(0));
}

template <typename Tree, bool Shuffled>
static void BM_Search(benchmark::State &state) {
  auto courses = makeCourses(state.range(0), Shuffled);
  Tree tree; // built once, outside the timed loop
  for (const auto &c : courses)
    tree.Insert(c);
  for (auto _ : state) {
    for (const auto &c : courses) {
      benchmark::DoNotOptimize(tree.Search(c.getKey()));
    }
  }
  state.SetComplexityN(state.range(0));
}

using BST = BinarySearchTree<Course>;
BENCHMARK_TEMPLATE(BM_Insert, BST, false)
    ->RangeMultiplier(2)
    ->Range(256, 8192)
    ->Complexity();
BENCHMARK_TEMPLATE(BM_Insert, BST, true)
    ->RangeMultiplier(2)
    ->Range(256, 8192)
    ->Complexity();
BENCHMARK_TEMPLATE(BM_Search, BST, false)
    ->RangeMultiplier(2)
    ->Range(256, 8192)
    ->Complexity();
BENCHMARK_TEMPLATE(BM_Search, BST, true)
    ->RangeMultiplier(2)
    ->Range(256, 8192)
    ->Complexity();

BENCHMARK_MAIN();
