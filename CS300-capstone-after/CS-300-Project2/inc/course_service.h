#pragma once
#ifndef COURSE_SERVICE_H
#define COURSE_SERVICE_H

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "avl.h"
#include "bst.h"
#include "course.h"
#include "course_repository.h"
#include "graph.h"

// A course that is malformed on its own (empty ID or title).
class InvalidCourseError : public std::invalid_argument {
public:
  using std::invalid_argument::invalid_argument;
};

// Which search tree caches the catalog in memory.
enum class TreeKind { AVL, BST };

/*
 * Business logic for the course planner: searching, validating changes, and
 * prerequisite planning.
 *
 * CourseService only knows ICourseRepository, never a concrete backend, so it
 * works the same over a CSV file, a SQLite database, or a test fake.
 *
 * The repository is the source of truth. The service keeps an in-memory cache
 * built from it (the AVL tree or BST from Enhancement 2, plus the prerequisite
 * graph) so lookups and planning don't hit storage. Every change is:
 *   1. validated against the whole catalog (including cycle detection, which
 *      a database constraint can't express),
 *   2. written to the repository (which applies it all-or-nothing),
 *   3. applied to the cache only after the write succeeds.
 * If step 1 or 2 throws, neither storage nor the cache changes.
 */
class CourseService {
public:
  explicit CourseService(ICourseRepository &repo,
                         TreeKind tree = TreeKind::AVL);

  // Not copyable: it refers to one repository and owns the cached trees.
  CourseService(const CourseService &) = delete;
  CourseService &operator=(const CourseService &) = delete;

  // (Re)builds the cache from the repository. Throws whatever the repository
  // throws (e.g. a CSV error with a line number) or CyclicPrereqException;
  // on failure the previous cache is kept.
  void Load();
  bool Loaded() const { return loaded_; }

  // ---- Queries
  // ---------------------------------------------------------------
  std::vector<Course> AllCoursesSorted() const;
  std::optional<Course> Find(const std::string &id) const; // id is normalized
  std::size_t CourseCount() const { return graph_.CourseCount(); }
  const PrereqGraph &Graph() const { return graph_; }

  // ---- Changes (throw on any problem; nothing changes in that case)
  // ----------
  bool CanEdit() const { return !repo_.IsReadOnly(); }
  void AddCourse(Course course);
  void UpdateCourse(Course course);
  void RemoveCourse(const std::string &id);

  // Validates every course from `source`, then replaces this repository's
  // catalog with it in one transaction. Returns the number of courses.
  std::size_t ImportFrom(ICourseRepository &source);

  // ---- Student progress
  // ------------------------------------------------------
  const std::set<std::string> &Completed() const { return completed_; }
  void MarkCompleted(const std::string &id, bool completed = true);
  // Courses not completed whose prerequisites are all completed.
  std::vector<Course> EligibleCourses() const;

  // ---- Search tree used for the cache
  // ----------------------------------------
  TreeKind Tree() const { return treeKind_; }
  void SetTree(TreeKind kind);
  const AVLTree<Course> &Avl() const { return avl_; }
  const BinarySearchTree<Course> &Bst() const { return bst_; }

  std::string StorageDescription() const { return repo_.Describe(); }

  // Trim and uppercase, so " csci100" and "CSCI100" are the same course.
  static std::string NormalizeId(const std::string &id);

  // Checks a whole catalog: IDs and titles present, no duplicates, no
  // self-prerequisites, every prerequisite exists, and no cycles. Throws the
  // first problem found. Returns the prerequisite graph it built.
  static PrereqGraph Validate(const std::vector<Course> &courses);

private:
  ICourseRepository &repo_;
  TreeKind treeKind_;
  AVLTree<Course> avl_;
  BinarySearchTree<Course> bst_;
  PrereqGraph graph_;
  std::set<std::string> completed_;
  bool loaded_ = false;

  static Course normalized(Course course);
  void rebuild(const std::vector<Course> &courses, PrereqGraph graph);
  void requireLoaded() const;
};

#endif // COURSE_SERVICE_H
