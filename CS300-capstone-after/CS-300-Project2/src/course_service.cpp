#include "course_service.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <utility>

#include "csv.h" // trim()

CourseService::CourseService(ICourseRepository &repo, TreeKind tree)
    : repo_(repo), treeKind_(tree) {}

// ---- Helpers
// -----------------------------------------------------------------

std::string CourseService::NormalizeId(const std::string &id) {
  std::string out = trim(id);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return out;
}

Course CourseService::normalized(Course course) {
  course.courseID = NormalizeId(course.courseID);
  course.courseTitle = trim(course.courseTitle);
  for (auto &p : course.prereq)
    p = NormalizeId(p);
  std::sort(course.prereq.begin(), course.prereq.end());
  course.prereq.erase(std::unique(course.prereq.begin(), course.prereq.end()),
                      course.prereq.end());
  return course;
}

PrereqGraph CourseService::Validate(const std::vector<Course> &courses) {
  std::unordered_set<std::string> ids;
  for (const auto &c : courses) {
    if (c.courseID.empty())
      throw InvalidCourseError("A course is missing its ID.");
    if (c.courseTitle.empty())
      throw InvalidCourseError("Course " + c.courseID + " is missing a title.");
    if (!ids.insert(c.courseID).second)
      throw DuplicateCourseError(c.courseID);
  }

  PrereqGraph graph;
  for (const auto &c : courses)
    graph.AddCourse(c.courseID);
  for (const auto &c : courses) {
    for (const auto &p : c.prereq) {
      if (p == c.courseID)
        throw SelfPrerequisiteError(c.courseID);
      if (!ids.count(p))
        throw UnknownCourseError(c.courseID, p);
      graph.AddEdge(p, c.courseID);
    }
  }

  graph.TopologicalOrder(); // throws CyclicPrereqException with the loop
  return graph;
}

void CourseService::rebuild(const std::vector<Course> &courses,
                            PrereqGraph graph) {
  avl_.Clear();
  bst_.Clear();
  for (const auto &c : courses) {
    if (treeKind_ == TreeKind::AVL)
      avl_.Insert(c);
    else
      bst_.Insert(c);
  }
  graph_ = std::move(graph);
  loaded_ = true;
}

void CourseService::requireLoaded() const {
  if (!loaded_)
    throw std::logic_error("Catalog has not been loaded yet.");
}

// ---- Loading
// -------------------------------------------------------------------

void CourseService::Load() {
  std::vector<Course> courses = repo_.LoadAll();
  for (auto &c : courses)
    c = normalized(std::move(c));
  std::sort(
      courses.begin(), courses.end(),
      [](const Course &a, const Course &b) { return a.courseID < b.courseID; });

  PrereqGraph graph = Validate(courses); // throws before anything changes
  std::vector<std::string> done = repo_.LoadCompleted();

  rebuild(courses, std::move(graph));
  completed_.clear();
  for (const auto &id : done)
    completed_.insert(NormalizeId(id));
}

// ---- Queries
// -------------------------------------------------------------------

std::vector<Course> CourseService::AllCoursesSorted() const {
  std::vector<Course> out;
  auto collect = [&out](const Course &c) { out.push_back(c); };
  if (treeKind_ == TreeKind::AVL)
    avl_.InOrder(collect);
  else
    bst_.InOrder(collect);
  return out;
}

std::optional<Course> CourseService::Find(const std::string &id) const {
  std::string key = NormalizeId(id);
  return treeKind_ == TreeKind::AVL ? avl_.Search(key) : bst_.Search(key);
}

// ---- Changes
// -------------------------------------------------------------------

void CourseService::AddCourse(Course course) {
  requireLoaded();
  course = normalized(std::move(course));

  std::vector<Course> candidate = AllCoursesSorted();
  candidate.push_back(course);
  PrereqGraph graph = Validate(candidate);

  repo_.Add(course); // storage first; the cache only follows a real write
  if (treeKind_ == TreeKind::AVL)
    avl_.Insert(course);
  else
    bst_.Insert(course);
  graph_ = std::move(graph);
}

void CourseService::UpdateCourse(Course course) {
  requireLoaded();
  course = normalized(std::move(course));

  std::vector<Course> candidate = AllCoursesSorted();
  auto it =
      std::find_if(candidate.begin(), candidate.end(), [&](const Course &c) {
        return c.courseID == course.courseID;
      });
  if (it == candidate.end())
    throw CourseNotFoundError(course.courseID);
  *it = course;
  PrereqGraph graph = Validate(candidate); // catches a new cycle

  repo_.Update(course);
  if (treeKind_ == TreeKind::AVL) {
    avl_.Remove(course.courseID);
    avl_.Insert(course);
  } else {
    bst_.Remove(course.courseID);
    bst_.Insert(course);
  }
  graph_ = std::move(graph);
}

void CourseService::RemoveCourse(const std::string &rawId) {
  requireLoaded();
  std::string id = NormalizeId(rawId);
  if (!graph_.Contains(id))
    throw CourseNotFoundError(id);
  if (auto deps = graph_.DirectlyUnlocks(id); !deps.empty())
    throw CourseInUseError(id, deps);

  repo_.Remove(id);

  std::vector<Course> remaining = AllCoursesSorted();
  std::erase_if(remaining, [&](const Course &c) { return c.courseID == id; });
  // PrereqGraph has no remove, and nothing depended on this course, so
  // rebuilding it from the remaining courses is simple and can't fail.
  rebuild(remaining, Validate(remaining));
  completed_.erase(id);
}

std::size_t CourseService::ImportFrom(ICourseRepository &source) {
  std::vector<Course> courses = source.LoadAll();
  for (auto &c : courses)
    c = normalized(std::move(c));
  std::sort(
      courses.begin(), courses.end(),
      [](const Course &a, const Course &b) { return a.courseID < b.courseID; });
  PrereqGraph graph = Validate(courses);

  repo_.ReplaceAll(courses); // one transaction: all or nothing
  rebuild(courses, std::move(graph));
  completed_.clear();
  return courses.size();
}

// ---- Student progress
// ------------------------------------------------------------

void CourseService::MarkCompleted(const std::string &rawId, bool completed) {
  requireLoaded();
  std::string id = NormalizeId(rawId);
  if (!graph_.Contains(id))
    throw CourseNotFoundError(id);

  repo_.SetCompleted(id, completed);
  if (completed)
    completed_.insert(id);
  else
    completed_.erase(id);
}

std::vector<Course> CourseService::EligibleCourses() const {
  std::vector<Course> out;
  for (const auto &c : AllCoursesSorted()) {
    if (completed_.count(c.courseID))
      continue;
    bool ready = std::all_of(
        c.prereq.begin(), c.prereq.end(),
        [&](const std::string &p) { return completed_.count(p) > 0; });
    if (ready)
      out.push_back(c);
  }
  return out;
}

// ---- Tree choice
// -----------------------------------------------------------------

void CourseService::SetTree(TreeKind kind) {
  if (kind == treeKind_)
    return;
  std::vector<Course> courses = AllCoursesSorted();
  treeKind_ = kind;
  if (loaded_) {
    avl_.Clear();
    bst_.Clear();
    for (const auto &c : courses) {
      if (kind == TreeKind::AVL)
        avl_.Insert(c);
      else
        bst_.Insert(c);
    }
  }
}
