#pragma once
#ifndef SQLITE_REPOSITORY_H
#define SQLITE_REPOSITORY_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "course_repository.h"

struct sqlite3; // from <sqlite3.h>; kept out of this header on purpose

/*
 * SQLite-backed repository: the program's durable source of truth.
 *
 * Schema (created on first open, version tracked with PRAGMA user_version):
 *
 *   courses(course_id TEXT PRIMARY KEY, title TEXT NOT NULL)
 *   prerequisites(course_id -> courses ON DELETE CASCADE,
 *                 prereq_id -> courses,
 *                 PRIMARY KEY (course_id, prereq_id),
 *                 CHECK (course_id <> prereq_id))
 *   completed_courses(course_id PRIMARY KEY -> courses ON DELETE CASCADE)
 *
 * The database enforces integrity itself, so even a buggy caller can't store
 * a duplicate course, an unknown prerequisite, or a course that requires
 * itself. Foreign keys are switched on for every connection.
 *
 * Every query is a prepared statement with bound parameters: user input is
 * never concatenated into SQL. Every write runs in a transaction, so a failed
 * write (or a failed CSV import halfway through) leaves the database exactly
 * as it was.
 *
 * Pass ":memory:" as the path for a throwaway in-memory database (tests).
 */
class SqliteRepository final : public ICourseRepository {
public:
  explicit SqliteRepository(const std::string &path);
  ~SqliteRepository() override;

  // One connection per object: not copyable, movable.
  SqliteRepository(const SqliteRepository &) = delete;
  SqliteRepository &operator=(const SqliteRepository &) = delete;
  SqliteRepository(SqliteRepository &&) noexcept;
  SqliteRepository &operator=(SqliteRepository &&) noexcept;

  std::vector<Course> LoadAll() override;
  std::optional<Course> Find(const std::string &id) override;
  void Add(const Course &course) override;
  void Update(const Course &course) override;
  void Remove(const std::string &id) override;
  void ReplaceAll(const std::vector<Course> &courses) override;

  std::vector<std::string> LoadCompleted() override;
  void SetCompleted(const std::string &id, bool completed) override;

  bool IsReadOnly() const override { return false; }
  std::string Describe() const override { return "SQLite: " + path_; }

  // SQLite-only query, answered entirely in SQL: courses not yet completed
  // whose every prerequisite is completed. Sorted by course ID. CourseService
  // computes the same thing in memory for any backend; the tests check that
  // the two always agree.
  std::vector<Course> EligibleCourses();

  static constexpr int kSchemaVersion = 1;

private:
  struct Closer {
    void operator()(sqlite3 *db) const;
  };
  std::unique_ptr<sqlite3, Closer> db_;
  std::string path_;

  void exec(const char *sql);
  void createSchema();
  bool exists(const std::string &id);
  void insertCourse(const Course &course);
  void insertPrereqs(const Course &course);
  std::vector<Course> query(const char *sql);
};

#endif // SQLITE_REPOSITORY_H
