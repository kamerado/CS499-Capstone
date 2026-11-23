#include "avl.h"
#include "bst.h"
#include "course.h"
#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstdio>
#include <format>
#include <map>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

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

// Height of any tree with Root() / left / right. Iterative, so it's safe on a
// degenerate BST chain.
template <typename NodeT> static int treeHeight(const NodeT *root) {
  int best = 0;
  std::vector<std::pair<const NodeT *, int>> stack;
  if (root)
    stack.push_back({root, 1});
  while (!stack.empty()) {
    auto [n, depth] = stack.back();
    stack.pop_back();
    best = std::max(best, depth);
    if (n->left)
      stack.push_back({n->left.get(), depth + 1});
    if (n->right)
      stack.push_back({n->right.get(), depth + 1});
  }
  return best;
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
  state.SetItemsProcessed(state.iterations() * state.range(0)); // inserts/sec
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
  state.SetItemsProcessed(state.iterations() * state.range(0)); // searches/sec
  state.counters["height"] = treeHeight(tree.Root());
}

// ---- BST (baseline) ----
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

// ---- AVL (enhancement) ----
using AVL = AVLTree<Course>;
BENCHMARK_TEMPLATE(BM_Insert, AVL, false)
    ->RangeMultiplier(2)
    ->Range(256, 8192)
    ->Complexity();
BENCHMARK_TEMPLATE(BM_Insert, AVL, true)
    ->RangeMultiplier(2)
    ->Range(256, 8192)
    ->Complexity();
BENCHMARK_TEMPLATE(BM_Search, AVL, false)
    ->RangeMultiplier(2)
    ->Range(256, 8192)
    ->Complexity();
BENCHMARK_TEMPLATE(BM_Search, AVL, true)
    ->RangeMultiplier(2)
    ->Range(256, 8192)
    ->Complexity();
// Extra bench to showcase larger trees.
BENCHMARK_TEMPLATE(BM_Insert, AVL, false)
    ->Name("BM_Insert<AVL, sorted>/large")
    ->RangeMultiplier(4)
    ->Range(8192, 1 << 20)
    ->Complexity();

// ---------------------------------------------------------------------------
// BST vs AVL comparison
// ---------------------------------------------------------------------------

/*
 * Prints the normal console output, and also records the time of every
 * benchmark run. After all benchmarks finish, PrintComparison() pairs each
 * BST result with the AVL result of the same name (e.g.
 * "BM_Insert<BST, false>/8192" with "BM_Insert<AVL, false>/8192") and prints
 * how much faster the AVL tree was.
 */
class ComparisonReporter : public benchmark::ConsoleReporter {
public:
  void ReportRuns(const std::vector<Run> &runs) override {
    ConsoleReporter::ReportRuns(runs); // keep the normal output
    for (const auto &run : runs) {
      // Only real timing rows: skip errors, BigO/RMS rows, and the
      // mean/median/stddev rows produced by --benchmark_repetitions.
      if (run.skipped || run.report_big_o || run.report_rms ||
          run.run_type != Run::RT_Iteration)
        continue;
      Result &r = results_[run.benchmark_name()];
      r.totalNs += run.GetAdjustedRealTime() * toNs(run.time_unit);
      r.count++; // averaged if --benchmark_repetitions is used
      if (auto it = run.counters.find("height"); it != run.counters.end())
        r.height = static_cast<int>(it->second.value);
    }
  }

  void PrintComparison() const {
    std::printf("\n%s\n", std::string(96, '=').c_str());
    std::printf("BST vs AVL (time per iteration; positive improvement = AVL "
                "faster)\n");
    std::printf("%s\n", std::string(96, '=').c_str());
    std::printf("%-30s %14s %14s %13s %9s %15s\n", "Benchmark", "BST", "AVL",
                "Improvement", "Speedup", "Height BST/AVL");
    std::printf("%s\n", std::string(96, '-').c_str());

    // Pair up "BM_<op><BST, <shuffled>>/<n>" with the matching AVL name.
    struct Row {
      std::string op;
      bool shuffled;
      long n;
      const Result *bst;
      const Result *avl;
    };
    std::vector<Row> rows;
    for (const auto &[name, bst] : results_) {
      std::size_t pos = name.find("<BST");
      if (pos == std::string::npos)
        continue;
      std::string avlName = name;
      avlName.replace(pos, 4, "<AVL");
      auto it = results_.find(avlName);
      if (it == results_.end())
        continue;
      rows.push_back({name.substr(3, pos - 3), // between "BM_" and "<"
                      name.find("true") != std::string::npos,
                      std::stol(name.substr(name.rfind('/') + 1)), &bst,
                      &it->second});
    }
    // Group by operation, then sorted before shuffled, then size (numeric).
    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
      return std::tie(a.op, a.shuffled, a.n) < std::tie(b.op, b.shuffled, b.n);
    });

    for (std::size_t i = 0; i < rows.size(); ++i) {
      const Row &r = rows[i];
      if (i > 0 &&
          (r.op != rows[i - 1].op || r.shuffled != rows[i - 1].shuffled))
        std::printf("\n"); // blank line between groups

      double bstNs = r.bst->totalNs / r.bst->count;
      double avlNs = r.avl->totalNs / r.avl->count;
      double improvement = (bstNs - avlNs) / bstNs * 100.0;
      double speedup = bstNs / avlNs;

      std::string label = std::format("{:<7}{:<10}n={}", r.op,
                                      r.shuffled ? "shuffled" : "sorted", r.n);
      std::string heights = "-";
      if (r.bst->height > 0 && r.avl->height > 0)
        heights = std::format("{}/{}", r.bst->height, r.avl->height);

      std::printf("%-30s %14s %14s %12.1f%% %8.2fx %15s\n", label.c_str(),
                  formatTime(bstNs).c_str(), formatTime(avlNs).c_str(),
                  improvement, speedup, heights.c_str());
    }
    std::printf("%s\n", std::string(96, '=').c_str());
  }

private:
  struct Result {
    double totalNs = 0;
    int count = 0;
    int height = 0;
  };
  std::map<std::string, Result> results_; // sorted by name

  static double toNs(benchmark::TimeUnit unit) {
    switch (unit) {
    case benchmark::kNanosecond:
      return 1.0;
    case benchmark::kMicrosecond:
      return 1e3;
    case benchmark::kMillisecond:
      return 1e6;
    case benchmark::kSecond:
      return 1e9;
    }
    return 1.0;
  }

  static std::string formatTime(double ns) {
    if (ns >= 1e9)
      return std::format("{:.2f} s", ns / 1e9);
    if (ns >= 1e6)
      return std::format("{:.2f} ms", ns / 1e6);
    if (ns >= 1e3)
      return std::format("{:.2f} us", ns / 1e3);
    return std::format("{:.0f} ns", ns);
  }
};

int main(int argc, char **argv) {
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;

  ComparisonReporter reporter;
  benchmark::RunSpecifiedBenchmarks(&reporter);
  reporter.PrintComparison();

  benchmark::Shutdown();
  return 0;
}
