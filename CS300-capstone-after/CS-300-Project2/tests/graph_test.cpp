// Unit tests for PrereqGraph (graph.h / graph.cpp).
//
// The key helper is IsValidOrder(): a topological order is correct if every
// course appears after all of its prerequisites. Tests check that property
// rather than one hard-coded order wherever more than one order is valid.

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "graph.h"

namespace {

using Ids = std::vector<std::string>;

// prereq -> course pairs.
struct Edge {
  std::string prereq, course;
};

PrereqGraph Build(const Ids &courses, const std::vector<Edge> &edges) {
  PrereqGraph g;
  for (const auto &c : courses)
    g.AddCourse(c);
  for (const auto &e : edges)
    g.AddEdge(e.prereq, e.course);
  return g;
}

// The ABCU sample catalog.
PrereqGraph SampleCatalog() {
  return Build({"CSCI100", "CSCI101", "CSCI200", "MATH201", "CSCI300",
                "CSCI301", "CSCI350", "CSCI400"},
               {{"CSCI100", "CSCI101"},
                {"CSCI101", "CSCI200"},
                {"CSCI200", "CSCI300"},
                {"MATH201", "CSCI300"},
                {"CSCI101", "CSCI301"},
                {"CSCI300", "CSCI350"},
                {"CSCI301", "CSCI400"},
                {"CSCI350", "CSCI400"}});
}

/*     A
 *    / \
 *   B   C
 *    \ /
 *     D
 */
PrereqGraph Diamond() {
  return Build({}, {{"A", "B"}, {"A", "C"}, {"B", "D"}, {"C", "D"}});
}

::testing::AssertionResult IsValidOrder(const PrereqGraph &g,
                                        const Ids &order) {
  if (order.size() != g.CourseCount())
    return ::testing::AssertionFailure()
           << "order has " << order.size() << " courses, graph has "
           << g.CourseCount();

  std::unordered_map<std::string, std::size_t> pos;
  for (std::size_t i = 0; i < order.size(); ++i)
    if (!pos.emplace(order[i], i).second)
      return ::testing::AssertionFailure() << order[i] << " appears twice";

  for (const auto &course : order)
    for (const auto &prereq : g.DirectPrereqs(course))
      if (pos.at(prereq) > pos.at(course))
        return ::testing::AssertionFailure()
               << prereq << " comes after " << course << ", which needs it";
  return ::testing::AssertionSuccess();
}

// Every consecutive pair in a reported cycle must be a real edge.
::testing::AssertionResult IsRealCycle(const PrereqGraph &g, const Ids &cycle) {
  if (cycle.size() < 2 || cycle.front() != cycle.back())
    return ::testing::AssertionFailure() << "not a closed cycle";
  for (std::size_t i = 0; i + 1 < cycle.size(); ++i) {
    Ids next = g.DirectlyUnlocks(cycle[i]);
    if (std::find(next.begin(), next.end(), cycle[i + 1]) == next.end())
      return ::testing::AssertionFailure()
             << "no edge " << cycle[i] << " -> " << cycle[i + 1];
  }
  return ::testing::AssertionSuccess();
}

} // namespace

// ---------------------------------------------------------------------------
// Building
// ---------------------------------------------------------------------------

TEST(GraphBuild, EmptyGraph) {
  PrereqGraph g;
  EXPECT_TRUE(g.Empty());
  EXPECT_EQ(g.CourseCount(), 0u);
  EXPECT_EQ(g.EdgeCount(), 0u);
  EXPECT_TRUE(g.TopologicalOrder().empty());
  EXPECT_TRUE(g.Levels().empty());
  EXPECT_FALSE(g.HasCycle());
}

TEST(GraphBuild, AddCourseAddsIsolatedNode) {
  PrereqGraph g;
  g.AddCourse("CS100");
  EXPECT_TRUE(g.Contains("CS100"));
  EXPECT_EQ(g.CourseCount(), 1u);
  EXPECT_TRUE(g.DirectPrereqs("CS100").empty());
  EXPECT_TRUE(g.DirectlyUnlocks("CS100").empty());
}

TEST(GraphBuild, AddingCourseTwiceKeepsItsEdges) {
  PrereqGraph g;
  g.AddEdge("CS100", "CS200");
  g.AddCourse("CS200");
  EXPECT_EQ(g.CourseCount(), 2u);
  EXPECT_EQ(g.DirectPrereqs("CS200"), Ids{"CS100"});
}

TEST(GraphBuild, EdgeRunsFromPrereqToCourse) {
  PrereqGraph g;
  g.AddEdge("CS100", "CS200"); // CS200 requires CS100

  EXPECT_EQ(g.DirectlyUnlocks("CS100"), Ids{"CS200"});
  EXPECT_EQ(g.DirectPrereqs("CS200"), Ids{"CS100"});
  EXPECT_TRUE(g.DirectPrereqs("CS100").empty());
  EXPECT_TRUE(g.DirectlyUnlocks("CS200").empty());
}

TEST(GraphBuild, AddEdgeAddsMissingCourses) {
  PrereqGraph g;
  g.AddEdge("CS100", "CS200");
  EXPECT_TRUE(g.Contains("CS100"));
  EXPECT_TRUE(g.Contains("CS200"));
  EXPECT_EQ(g.CourseCount(), 2u);
  EXPECT_EQ(g.EdgeCount(), 1u);
}

TEST(GraphBuild, DuplicateEdgeIsIgnored) {
  PrereqGraph g;
  g.AddEdge("CS100", "CS200");
  g.AddEdge("CS100", "CS200");
  EXPECT_EQ(g.EdgeCount(), 1u);
  EXPECT_EQ(g.DirectPrereqs("CS200"), Ids{"CS100"});
}

TEST(GraphBuild, SampleCatalogCounts) {
  PrereqGraph g = SampleCatalog();
  EXPECT_EQ(g.CourseCount(), 8u);
  EXPECT_EQ(g.EdgeCount(), 8u);
  EXPECT_EQ(g.DirectPrereqs("CSCI300"), (Ids{"CSCI200", "MATH201"}));
  EXPECT_EQ(g.DirectlyUnlocks("CSCI101"), (Ids{"CSCI200", "CSCI301"}));
}

TEST(GraphBuild, ClearEmptiesAndStaysUsable) {
  PrereqGraph g = SampleCatalog();
  g.Clear();
  EXPECT_TRUE(g.Empty());
  EXPECT_EQ(g.EdgeCount(), 0u);

  g.AddEdge("X", "Y");
  EXPECT_EQ(g.TopologicalOrder(), (Ids{"X", "Y"}));
}

TEST(GraphBuild, UnknownCourseQueriesAreEmpty) {
  PrereqGraph g = SampleCatalog();
  EXPECT_FALSE(g.Contains("NOPE100"));
  EXPECT_TRUE(g.DirectPrereqs("NOPE100").empty());
  EXPECT_TRUE(g.DirectlyUnlocks("NOPE100").empty());
  EXPECT_TRUE(g.AllPrereqs("NOPE100").empty());
  EXPECT_TRUE(g.AllUnlocks("NOPE100").empty());
}

// ---------------------------------------------------------------------------
// Reachability
// ---------------------------------------------------------------------------

TEST(GraphReach, AllPrereqsIncludesIndirect) {
  PrereqGraph g = SampleCatalog();
  EXPECT_EQ(g.AllPrereqs("CSCI300"),
            (Ids{"CSCI100", "CSCI101", "CSCI200", "MATH201"}));
}

TEST(GraphReach, AllPrereqsOfFinalCourseIsEverythingElse) {
  PrereqGraph g = SampleCatalog();
  EXPECT_EQ(g.AllPrereqs("CSCI400"),
            (Ids{"CSCI100", "CSCI101", "CSCI200", "CSCI300", "CSCI301",
                 "CSCI350", "MATH201"}));
}

TEST(GraphReach, EntryCourseHasNoPrereqs) {
  PrereqGraph g = SampleCatalog();
  EXPECT_TRUE(g.AllPrereqs("CSCI100").empty());
  EXPECT_TRUE(g.AllPrereqs("MATH201").empty());
}

TEST(GraphReach, AllUnlocksFollowsEdgesForward) {
  PrereqGraph g = SampleCatalog();
  EXPECT_EQ(g.AllUnlocks("MATH201"), (Ids{"CSCI300", "CSCI350", "CSCI400"}));
  EXPECT_TRUE(g.AllUnlocks("CSCI400").empty());
}

TEST(GraphReach, DiamondReportsEachCourseOnce) {
  PrereqGraph g = Diamond();
  EXPECT_EQ(g.AllPrereqs("D"), (Ids{"A", "B", "C"}));
  EXPECT_EQ(g.AllUnlocks("A"), (Ids{"B", "C", "D"}));
}

TEST(GraphReach, TerminatesOnCycle) {
  PrereqGraph g = Build({}, {{"A", "B"}, {"B", "C"}, {"C", "A"}, {"X", "A"}});
  EXPECT_EQ(g.AllPrereqs("B"), (Ids{"A", "C", "X"}));
}

// ---------------------------------------------------------------------------
// Topological order
// ---------------------------------------------------------------------------

TEST(GraphOrder, SampleCatalogOrderIsValid) {
  PrereqGraph g = SampleCatalog();
  EXPECT_TRUE(IsValidOrder(g, g.TopologicalOrder()));
}

// Ties are broken alphabetically, so the exact order is predictable.
TEST(GraphOrder, SampleCatalogExactOrder) {
  PrereqGraph g = SampleCatalog();
  EXPECT_EQ(g.TopologicalOrder(),
            (Ids{"CSCI100", "CSCI101", "CSCI200", "CSCI301", "MATH201",
                 "CSCI300", "CSCI350", "CSCI400"}));
}

TEST(GraphOrder, IsolatedCoursesAreIncluded) {
  PrereqGraph g = Build({"ART100", "Z900"}, {{"CS100", "CS200"}});
  Ids order = g.TopologicalOrder();
  EXPECT_TRUE(IsValidOrder(g, order));
  EXPECT_EQ(order, (Ids{"ART100", "CS100", "CS200", "Z900"}));
}

TEST(GraphOrder, LongChain) {
  PrereqGraph g;
  for (int i = 0; i < 1000; ++i)
    g.AddEdge("C" + std::to_string(10000 + i), "C" + std::to_string(10001 + i));
  Ids order = g.TopologicalOrder();
  EXPECT_TRUE(IsValidOrder(g, order));
  EXPECT_EQ(order.front(), "C10000");
  EXPECT_EQ(order.back(), "C11000");
}

TEST(GraphOrder, InsertionOrderDoesNotChangeResult) {
  PrereqGraph a = SampleCatalog();

  PrereqGraph b; // same edges, added in reverse
  std::vector<Edge> edges = {{"CSCI350", "CSCI400"}, {"CSCI301", "CSCI400"},
                             {"CSCI300", "CSCI350"}, {"CSCI101", "CSCI301"},
                             {"MATH201", "CSCI300"}, {"CSCI200", "CSCI300"},
                             {"CSCI101", "CSCI200"}, {"CSCI100", "CSCI101"}};
  for (const auto &e : edges)
    b.AddEdge(e.prereq, e.course);

  EXPECT_EQ(a.TopologicalOrder(), b.TopologicalOrder());
}

// Random DAGs: edges only go from a lower to a higher index, so no cycles.
TEST(GraphOrder, RandomDAGsGiveValidOrders) {
  std::mt19937 rng(42);
  for (int round = 0; round < 20; ++round) {
    PrereqGraph g;
    const int n = 200;
    for (int i = 0; i < n; ++i)
      g.AddCourse("N" + std::to_string(1000 + i));
    for (int e = 0; e < 600; ++e) {
      int a = static_cast<int>(rng() % n), b = static_cast<int>(rng() % n);
      if (a == b)
        continue;
      if (a > b)
        std::swap(a, b);
      g.AddEdge("N" + std::to_string(1000 + a), "N" + std::to_string(1000 + b));
    }
    ASSERT_FALSE(g.HasCycle()) << "round " << round;
    ASSERT_TRUE(IsValidOrder(g, g.TopologicalOrder())) << "round " << round;
  }
}

// ---------------------------------------------------------------------------
// Levels (course plan by term)
// ---------------------------------------------------------------------------

TEST(GraphLevels, SampleCatalogLevels) {
  PrereqGraph g = SampleCatalog();
  auto levels = g.Levels();
  ASSERT_EQ(levels.size(), 6u);
  EXPECT_EQ(levels[0], (Ids{"CSCI100", "MATH201"}));
  EXPECT_EQ(levels[1], (Ids{"CSCI101"}));
  EXPECT_EQ(levels[2], (Ids{"CSCI200", "CSCI301"}));
  EXPECT_EQ(levels[3], (Ids{"CSCI300"}));
  EXPECT_EQ(levels[4], (Ids{"CSCI350"}));
  EXPECT_EQ(levels[5], (Ids{"CSCI400"}));
}

// CSCI400 needs CSCI350 (level 4), so it must be one level later.
TEST(GraphLevels, LevelIsOneMoreThanDeepestPrereq) {
  PrereqGraph g = SampleCatalog();
  auto levels = g.Levels();
  std::unordered_map<std::string, std::size_t> levelOf;
  for (std::size_t i = 0; i < levels.size(); ++i)
    for (const auto &id : levels[i])
      levelOf[id] = i;

  for (const auto &[id, lv] : levelOf) {
    std::size_t expected = 0;
    for (const auto &p : g.DirectPrereqs(id))
      expected = std::max(expected, levelOf.at(p) + 1);
    EXPECT_EQ(lv, expected) << id;
  }
}

TEST(GraphLevels, EveryCourseAppearsOnce) {
  PrereqGraph g = SampleCatalog();
  std::unordered_set<std::string> seen;
  for (const auto &group : g.Levels())
    for (const auto &id : group)
      EXPECT_TRUE(seen.insert(id).second) << id;
  EXPECT_EQ(seen.size(), g.CourseCount());
}

// ---------------------------------------------------------------------------
// Cycles
// ---------------------------------------------------------------------------

TEST(GraphCycle, AcyclicGraphHasNoCycle) {
  EXPECT_FALSE(SampleCatalog().HasCycle());
  EXPECT_TRUE(SampleCatalog().FindCycle().empty());
}

// The classic false positive: two paths to D is not a cycle.
TEST(GraphCycle, DiamondIsNotACycle) {
  PrereqGraph g = Diamond();
  EXPECT_FALSE(g.HasCycle());
  EXPECT_NO_THROW(g.TopologicalOrder());
}

TEST(GraphCycle, TwoCourseCycle) {
  PrereqGraph g = Build({}, {{"A", "B"}, {"B", "A"}});
  Ids cycle = g.FindCycle();
  EXPECT_EQ(cycle, (Ids{"A", "B", "A"}));
  EXPECT_TRUE(IsRealCycle(g, cycle));
}

TEST(GraphCycle, SelfLoopIsACycle) {
  PrereqGraph g;
  g.AddEdge("A", "A");
  EXPECT_EQ(g.FindCycle(), (Ids{"A", "A"}));
}

TEST(GraphCycle, ThreeCourseCycleIsReportedInEdgeDirection) {
  PrereqGraph g = Build({}, {{"A", "B"}, {"B", "C"}, {"C", "A"}});
  Ids cycle = g.FindCycle();
  ASSERT_EQ(cycle.size(), 4u);
  EXPECT_TRUE(IsRealCycle(g, cycle));
}

// Only the courses actually in the loop are reported, not ones that merely
// depend on it.
TEST(GraphCycle, ReportsOnlyTheLoop) {
  PrereqGraph g = Build(
      {"OK100"},
      {{"OK100", "B"}, {"B", "C"}, {"C", "D"}, {"D", "B"}, {"D", "AFTER"}});
  Ids cycle = g.FindCycle();
  EXPECT_TRUE(IsRealCycle(g, cycle));
  std::unordered_set<std::string> members(cycle.begin(), cycle.end());
  EXPECT_EQ(members, (std::unordered_set<std::string>{"B", "C", "D"}));
}

TEST(GraphCycle, TopologicalOrderThrowsWithCycleInMessage) {
  PrereqGraph g = Build({}, {{"CS100", "CS200"}, {"CS200", "CS100"}});
  try {
    g.TopologicalOrder();
    FAIL() << "expected CyclicPrereqException";
  } catch (const CyclicPrereqException &e) {
    EXPECT_STREQ(e.what(), "Circular prerequisites: CS100 -> CS200 -> CS100");
    EXPECT_EQ(e.cycle(), (Ids{"CS100", "CS200", "CS100"}));
  }
}

TEST(GraphCycle, LevelsThrowOnCycle) {
  PrereqGraph g = Build({}, {{"A", "B"}, {"B", "A"}});
  EXPECT_THROW(g.Levels(), CyclicPrereqException);
}

TEST(GraphCycle, ExceptionIsAStdException) {
  PrereqGraph g = Build({}, {{"A", "B"}, {"B", "A"}});
  EXPECT_THROW(g.TopologicalOrder(), std::runtime_error);
}

// ---------------------------------------------------------------------------
// DOT export
// ---------------------------------------------------------------------------

TEST(GraphDot, ContainsEveryEdge) {
  PrereqGraph g = SampleCatalog();
  std::ostringstream out;
  g.WriteDot(out);
  std::string dot = out.str();

  EXPECT_EQ(dot.rfind("digraph", 0), 0u);
  EXPECT_NE(dot.find("\"MATH201\" -> \"CSCI300\""), std::string::npos);
  EXPECT_NE(dot.find("\"CSCI350\" -> \"CSCI400\""), std::string::npos);
  std::size_t arrows = 0;
  for (std::size_t p = dot.find("->"); p != std::string::npos;
       p = dot.find("->", p + 2))
    ++arrows;
  EXPECT_EQ(arrows, g.EdgeCount());
}

TEST(GraphDot, WorksWithCycle) {
  PrereqGraph g = Build({}, {{"A", "B"}, {"B", "A"}});
  std::ostringstream out;
  EXPECT_NO_THROW(g.WriteDot(out));
  EXPECT_NE(out.str().find("\"B\" -> \"A\""), std::string::npos);
}
