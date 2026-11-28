#pragma once
#ifndef FAKE_REPOSITORY_H
#define FAKE_REPOSITORY_H

// An in-memory ICourseRepository for tests. It follows the same contract as
// SqliteRepository (the contract tests in repository_test.cpp run against
// both), and adds hooks to count writes and to make the next write fail, so
// the service tests can check what happens when storage breaks.

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

#include "course_repository.h"

class InMemoryRepository : public ICourseRepository {
public:
  std::vector<Course> LoadAll() override {
    std::vector<Course> out;
    for (const auto &[id, c] : courses_)
      out.push_back(c);
    return out;
  }

  std::optional<Course> Find(const std::string &id) override {
    auto it = courses_.find(id);
    if (it == courses_.end())
      return std::nullopt;
    return it->second;
  }

  void Add(const Course &course) override {
    beginWrite();
    if (courses_.count(course.courseID))
      throw DuplicateCourseError(course.courseID);
    checkPrereqs(course, courses_);
    courses_[course.courseID] = sorted(course);
  }

  void Update(const Course &course) override {
    beginWrite();
    if (!courses_.count(course.courseID))
      throw CourseNotFoundError(course.courseID);
    checkPrereqs(course, courses_);
    courses_[course.courseID] = sorted(course);
  }

  void Remove(const std::string &id) override {
    beginWrite();
    if (!courses_.count(id))
      throw CourseNotFoundError(id);
    std::vector<std::string> deps;
    for (const auto &[cid, c] : courses_)
      if (std::count(c.prereq.begin(), c.prereq.end(), id))
        deps.push_back(cid);
    if (!deps.empty())
      throw CourseInUseError(id, deps);
    courses_.erase(id);
    completed_.erase(id);
  }

  void ReplaceAll(const std::vector<Course> &courses) override {
    beginWrite();
    std::map<std::string, Course> next; // built aside: all-or-nothing
    for (const auto &c : courses)
      if (!next.emplace(c.courseID, sorted(c)).second)
        throw DuplicateCourseError(c.courseID);
    for (const auto &c : courses)
      checkPrereqs(c, next);
    courses_ = std::move(next);
    completed_.clear();
  }

  std::vector<std::string> LoadCompleted() override {
    return {completed_.begin(), completed_.end()};
  }

  void SetCompleted(const std::string &id, bool completed) override {
    beginWrite();
    if (!courses_.count(id))
      throw CourseNotFoundError(id);
    if (completed)
      completed_.insert(id);
    else
      completed_.erase(id);
  }

  bool IsReadOnly() const override { return false; }
  std::string Describe() const override { return "in-memory test repository"; }

  // ---- Test hooks ----
  int writes = 0;             // write calls that reached storage
  bool failNextWrite = false; // simulate a disk/database failure

private:
  std::map<std::string, Course> courses_;
  std::set<std::string> completed_;

  void beginWrite() {
    if (failNextWrite) {
      failNextWrite = false;
      throw RepositoryError("simulated storage failure");
    }
    ++writes;
  }

  static Course sorted(Course c) {
    std::sort(c.prereq.begin(), c.prereq.end());
    c.prereq.erase(std::unique(c.prereq.begin(), c.prereq.end()),
                   c.prereq.end());
    return c;
  }

  static void checkPrereqs(const Course &c,
                           const std::map<std::string, Course> &all) {
    for (const auto &p : c.prereq) {
      if (p == c.courseID)
        throw SelfPrerequisiteError(c.courseID);
      if (!all.count(p))
        throw UnknownCourseError(c.courseID, p);
    }
  }
};

#endif // FAKE_REPOSITORY_H
