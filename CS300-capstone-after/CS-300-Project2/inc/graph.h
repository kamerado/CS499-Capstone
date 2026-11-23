#pragma once
#ifndef GRAPH_H
#define GRAPH_H

#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

/*
 * Thrown when the prerequisites form a cycle (A -> B -> ... -> A), which means
 * none of the courses in the cycle can ever be taken. Derives from
 * std::runtime_error so an existing catch (const std::exception &) handles it.
 */
class CyclicPrereqException : public std::runtime_error {
public:
  explicit CyclicPrereqException(std::vector<std::string> cycle);
  const std::vector<std::string> &cycle() const { return cycle_; }

private:
  std::vector<std::string> cycle_; // first course repeated at the end
};

/*
 * Directed prerequisite graph. Each edge runs from a prerequisite to the
 * course that needs it, so "CSCI200 requires CSCI101" is CSCI101 -> CSCI200.
 *
 * Stored as adjacency lists in both directions (it's sparse: each course has
 * only a few prerequisites). The graph only knows course IDs, so it doesn't
 * depend on Course, the trees, or the CSV code.
 *
 * Every list a query returns is sorted, so results are the same on every run.
 */
class PrereqGraph {
public:
  // Building
  // Adds a course with no edges. Adding an existing course does nothing.
  void AddCourse(const std::string &id);
  // Adds prereq -> course. Missing courses are added. Duplicate edges are
  // ignored, so "CS200,DS,CS100,CS100" still gives one edge.
  void AddEdge(const std::string &prereq, const std::string &course);
  void Clear();

  // Basic queries
  bool Contains(const std::string &id) const;
  std::size_t CourseCount() const { return out_.size(); }
  std::size_t EdgeCount() const { return edgeCount_; }
  bool Empty() const { return out_.empty(); }

  // Direct neighbors. Unknown course -> empty list.
  std::vector<std::string> DirectPrereqs(const std::string &id) const;
  std::vector<std::string> DirectlyUnlocks(const std::string &id) const;

  // Everything reachable backwards (all prerequisites, direct and indirect)
  // or forwards (every course this one eventually leads to). O(V + E).
  std::vector<std::string> AllPrereqs(const std::string &id) const;
  std::vector<std::string> AllUnlocks(const std::string &id) const;

  // Ordering
  // Kahn's algorithm, O((V + E) log V) with alphabetical tie-breaking.
  // Every course appears after all of its prerequisites.
  // Throws CyclicPrereqException if there is a cycle.
  std::vector<std::string> TopologicalOrder() const;

  // Courses grouped into "terms": a course's level is 1 + the highest level
  // of its prerequisites, so everything in a level can be taken together once
  // the earlier levels are done. Throws CyclicPrereqException on a cycle.
  std::vector<std::vector<std::string>> Levels() const;

  // Empty if the graph is acyclic. Otherwise one cycle, in edge direction,
  // with the first course repeated at the end: {A, B, C, A}.
  std::vector<std::string> FindCycle() const;
  bool HasCycle() const { return !FindCycle().empty(); }

  // Export
  // Graphviz DOT, with courses on the same level drawn in the same row.
  void WriteDot(std::ostream &out) const;

private:
  using AdjList = std::unordered_map<std::string, std::vector<std::string>>;

  AdjList out_; // prereq -> courses it unlocks
  AdjList in_;  // course -> its direct prereqs
  std::size_t edgeCount_ = 0;

  struct KahnResult {
    std::vector<std::string> order;
    std::unordered_map<std::string, int> remaining; // in-degree left over
  };
  KahnResult runKahn() const;
  std::vector<std::string> cycleFrom(const KahnResult &kahn) const;
  std::vector<std::string> reachable(const std::string &id,
                                     const AdjList &edges) const;
  std::vector<std::string> sortedKeys() const;
};

#endif
