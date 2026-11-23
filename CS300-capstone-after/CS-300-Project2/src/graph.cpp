#include "graph.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <unordered_set>

namespace {

std::string joinCycle(const std::vector<std::string> &cycle) {
  std::string s;
  for (std::size_t i = 0; i < cycle.size(); ++i) {
    if (i)
      s += " -> ";
    s += cycle[i];
  }
  return s;
}

std::vector<std::string> sortedCopy(std::vector<std::string> v) {
  std::sort(v.begin(), v.end());
  return v;
}

} // namespace

CyclicPrereqException::CyclicPrereqException(std::vector<std::string> cycle)
    : std::runtime_error("Circular prerequisites: " + joinCycle(cycle)),
      cycle_(std::move(cycle)) {}

// Building
void PrereqGraph::AddCourse(const std::string &id) {
  out_.try_emplace(id); // no-op if it already exists
  in_.try_emplace(id);
}

void PrereqGraph::AddEdge(const std::string &prereq,
                          const std::string &course) {
  AddCourse(prereq);
  AddCourse(course);

  auto &unlocks = out_[prereq];
  if (std::find(unlocks.begin(), unlocks.end(), course) != unlocks.end())
    return; // duplicate edge

  unlocks.push_back(course);
  in_[course].push_back(prereq);
  ++edgeCount_;
}

void PrereqGraph::Clear() {
  out_.clear();
  in_.clear();
  edgeCount_ = 0;
}

// Basic queries
bool PrereqGraph::Contains(const std::string &id) const {
  return out_.count(id) != 0;
}

std::vector<std::string>
PrereqGraph::DirectPrereqs(const std::string &id) const {
  auto it = in_.find(id);
  return it == in_.end() ? std::vector<std::string>{} : sortedCopy(it->second);
}

std::vector<std::string>
PrereqGraph::DirectlyUnlocks(const std::string &id) const {
  auto it = out_.find(id);
  return it == out_.end() ? std::vector<std::string>{} : sortedCopy(it->second);
}

// Iterative DFS with a visited set: each course is reported once even when
// several paths lead to it (the "diamond" case), and it can't loop forever
// even if the graph has a cycle.
std::vector<std::string> PrereqGraph::reachable(const std::string &id,
                                                const AdjList &edges) const {
  std::vector<std::string> result;
  auto start = edges.find(id);
  if (start == edges.end())
    return result;

  std::unordered_set<std::string> visited{id};
  std::vector<std::string> stack(start->second.begin(), start->second.end());
  while (!stack.empty()) {
    std::string cur = std::move(stack.back());
    stack.pop_back();
    if (!visited.insert(cur).second)
      continue;
    result.push_back(cur);
    for (const auto &next : edges.at(cur))
      stack.push_back(next);
  }
  // A cycle back to the start course would add it; it isn't its own prereq
  // in any useful sense, and FindCycle reports that problem separately.
  return sortedCopy(std::move(result));
}

std::vector<std::string> PrereqGraph::AllPrereqs(const std::string &id) const {
  return reachable(id, in_);
}

std::vector<std::string> PrereqGraph::AllUnlocks(const std::string &id) const {
  return reachable(id, out_);
}

std::vector<std::string> PrereqGraph::sortedKeys() const {
  std::vector<std::string> keys;
  keys.reserve(out_.size());
  for (const auto &[id, _] : out_)
    keys.push_back(id);
  std::sort(keys.begin(), keys.end());
  return keys;
}

// Ordering
// Kahn's algorithm. A min-heap instead of a plain queue picks the
// alphabetically smallest available course each time, so the order is
// deterministic (unordered_map iteration order is not).
PrereqGraph::KahnResult PrereqGraph::runKahn() const {
  KahnResult r;
  for (const auto &[id, prereqs] : in_)
    r.remaining[id] = static_cast<int>(prereqs.size());

  std::priority_queue<std::string, std::vector<std::string>,
                      std::greater<std::string>>
      ready;
  for (const auto &[id, deg] : r.remaining)
    if (deg == 0)
      ready.push(id);

  while (!ready.empty()) {
    std::string cur = ready.top();
    ready.pop();
    r.order.push_back(cur);
    for (const auto &next : out_.at(cur))
      if (--r.remaining[next] == 0)
        ready.push(next);
  }
  return r;
}

// After Kahn's algorithm stops early, every unprocessed course still has an
// unprocessed prerequisite. So starting anywhere among them and repeatedly
// stepping to an unprocessed prerequisite must eventually revisit a course;
// the loop from that course back to itself is a cycle. O(V + E).
std::vector<std::string> PrereqGraph::cycleFrom(const KahnResult &kahn) const {
  if (kahn.order.size() == out_.size())
    return {};

  auto stuck = [&](const std::string &id) { return kahn.remaining.at(id) > 0; };

  std::string start;
  for (const auto &id : sortedKeys()) // smallest stuck course: deterministic
    if (stuck(id)) {
      start = id;
      break;
    }

  std::vector<std::string> path;
  std::unordered_map<std::string, std::size_t> seenAt;
  std::string cur = start;
  while (!seenAt.count(cur)) {
    seenAt[cur] = path.size();
    path.push_back(cur);
    std::string next;
    for (const auto &p : sortedCopy(in_.at(cur)))
      if (stuck(p)) {
        next = p;
        break;
      }
    cur = next;
  }

  // path walked prerequisite edges backwards; reverse into edge direction.
  std::vector<std::string> cycle(path.begin() + seenAt[cur], path.end());
  std::reverse(cycle.begin(), cycle.end());
  // Start at the alphabetically first course so the message is stable.
  std::rotate(cycle.begin(), std::min_element(cycle.begin(), cycle.end()),
              cycle.end());
  cycle.push_back(cycle.front());
  return cycle;
}

std::vector<std::string> PrereqGraph::TopologicalOrder() const {
  KahnResult kahn = runKahn();
  if (kahn.order.size() != out_.size())
    throw CyclicPrereqException(cycleFrom(kahn));
  return kahn.order;
}

std::vector<std::string> PrereqGraph::FindCycle() const {
  return cycleFrom(runKahn());
}

std::vector<std::vector<std::string>> PrereqGraph::Levels() const {
  std::unordered_map<std::string, std::size_t> level;
  std::vector<std::vector<std::string>> levels;

  // Topological order guarantees every prereq's level is known first.
  for (const auto &id : TopologicalOrder()) {
    std::size_t lv = 0;
    for (const auto &p : in_.at(id))
      lv = std::max(lv, level[p] + 1);
    level[id] = lv;
    if (levels.size() <= lv)
      levels.resize(lv + 1);
    levels[lv].push_back(id);
  }
  // Topological order isn't alphabetical within a level, so sort each one.
  for (auto &group : levels)
    std::sort(group.begin(), group.end());
  return levels;
}

// Export
void PrereqGraph::WriteDot(std::ostream &out) const {
  out << "digraph Prerequisites {\n";
  out << "  rankdir=LR;\n"; // prereqs on the left, later courses to the right
  out << "  node [shape=box, style=rounded, fontname=\"Helvetica\"];\n";

  // Put each level in its own column when the graph is acyclic.
  if (!HasCycle()) {
    for (const auto &group : Levels()) {
      out << "  { rank=same;";
      for (const auto &id : group)
        out << " \"" << id << "\";";
      out << " }\n";
    }
  } else {
    for (const auto &id : sortedKeys())
      out << "  \"" << id << "\";\n";
  }

  for (const auto &from : sortedKeys())
    for (const auto &to : DirectlyUnlocks(from))
      out << "  \"" << from << "\" -> \"" << to << "\";\n";
  out << "}\n";
}
