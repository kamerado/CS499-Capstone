#pragma once
#ifndef COURSE_REPOSITORY_H
#define COURSE_REPOSITORY_H

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "course.h"

/*
 * Storage interface for the course catalog.
 *
 * Everything above the storage layer (CourseService and the CLI) talks to
 * this abstract base class and never to a concrete backend. A backend only
 * stores and returns courses; whether a catalog makes sense (unknown
 * prerequisites, cycles) is the service's job. That keeps every backend small
 * and lets a new one (CSV today, SQLite now, maybe a REST API later) plug in
 * without touching the service or the menu. This is the dependency-inversion
 * principle: both layers depend on this abstraction instead of on each other.
 *
 * Contract every implementation follows:
 *   - Course IDs are stored exactly as given. The service normalizes them
 *     (trim + uppercase) before they reach a repository.
 *   - LoadAll() returns courses sorted by ID, and each course's prerequisites
 *     sorted by ID, so results are the same on every run and every backend.
 *   - Errors are reported with the exceptions below, never by exiting.
 *   - Writes are all-or-nothing: if a write throws, storage is unchanged.
 *   - A read-only backend throws ReadOnlyRepositoryError from every write.
 */
class ICourseRepository {
public:
  virtual ~ICourseRepository() = default;

  // ---- Courses -------------------------------------------------------------
  virtual std::vector<Course> LoadAll() = 0;
  virtual std::optional<Course> Find(const std::string &id) = 0;

  // Throws DuplicateCourseError if the ID exists, UnknownCourseError if a
  // prerequisite isn't a stored course.
  virtual void Add(const Course &course) = 0;

  // Replaces the title and prerequisites. Throws CourseNotFoundError if the
  // course doesn't exist, UnknownCourseError for an unknown prerequisite.
  virtual void Update(const Course &course) = 0;

  // Throws CourseNotFoundError if missing, CourseInUseError if another
  // course still lists it as a prerequisite.
  virtual void Remove(const std::string &id) = 0;

  // Atomically replaces the whole catalog (used to import a CSV file). The
  // completed-course list is cleared, since it may refer to old courses.
  virtual void ReplaceAll(const std::vector<Course> &courses) = 0;

  // ---- Courses the student has completed -----------------------------------
  virtual std::vector<std::string> LoadCompleted() = 0; // sorted
  // Throws CourseNotFoundError if the course doesn't exist.
  virtual void SetCompleted(const std::string &id, bool completed) = 0;

  // ---- Capabilities --------------------------------------------------------
  virtual bool IsReadOnly() const = 0;
  // Short human-readable name for the menu, e.g. "SQLite: catalog.db".
  virtual std::string Describe() const = 0;

protected:
  // Only derived classes are constructed; copying through the base would
  // slice, so the base isn't copyable on its own.
  ICourseRepository() = default;
  ICourseRepository(const ICourseRepository &) = default;
  ICourseRepository &operator=(const ICourseRepository &) = default;
};

// ---- Errors ----------------------------------------------------------------
// All derive from std::runtime_error, so the CLI's catch (const
// std::exception &) still reports anything unexpected.

class RepositoryError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class ReadOnlyRepositoryError : public RepositoryError {
public:
  ReadOnlyRepositoryError()
      : RepositoryError("This catalog is read-only. Open a database to edit "
                        "courses.") {}
};

class CourseNotFoundError : public RepositoryError {
public:
  explicit CourseNotFoundError(const std::string &id)
      : RepositoryError("Course " + id + " does not exist.") {}
};

class DuplicateCourseError : public RepositoryError {
public:
  explicit DuplicateCourseError(const std::string &id)
      : RepositoryError("Course " + id + " already exists.") {}
};

class UnknownCourseError : public RepositoryError {
public:
  UnknownCourseError(const std::string &course, const std::string &prereq)
      : RepositoryError(course + " lists unknown prerequisite " + prereq +
                        ".") {}
};

class CourseInUseError : public RepositoryError {
public:
  CourseInUseError(const std::string &id,
                   const std::vector<std::string> &dependents)
      : RepositoryError(message(id, dependents)), dependents_(dependents) {}
  const std::vector<std::string> &dependents() const { return dependents_; }

private:
  std::vector<std::string> dependents_;
  static std::string message(const std::string &id,
                             const std::vector<std::string> &dependents) {
    std::string msg = "Course " + id + " is a prerequisite of";
    for (const auto &d : dependents)
      msg += " " + d;
    return msg + ". Remove it from those courses first.";
  }
};

class SelfPrerequisiteError : public RepositoryError {
public:
  explicit SelfPrerequisiteError(const std::string &id)
      : RepositoryError("Course " + id + " cannot be its own prerequisite.") {}
};

#endif // COURSE_REPOSITORY_H
