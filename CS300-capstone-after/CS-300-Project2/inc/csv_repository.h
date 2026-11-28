#pragma once
#ifndef CSV_REPOSITORY_H
#define CSV_REPOSITORY_H

#include <optional>
#include <string>
#include <vector>

#include "course_repository.h"

/*
 * Read-only repository over the original ABCU CSV format:
 *   ID, Title, Prereq1, Prereq2, ...
 *
 * Parsing and row validation are the existing csv.cpp functions, so a bad
 * file still fails with the same line-numbered CSV exceptions. The file is
 * read on the first LoadAll()/Find() and cached after that.
 *
 * CSV has nowhere to record edits or completed courses, so every write throws
 * ReadOnlyRepositoryError. To edit, import the CSV into a SqliteRepository.
 */
class CsvRepository final : public ICourseRepository {
public:
  explicit CsvRepository(std::string path);

  std::vector<Course> LoadAll() override;
  std::optional<Course> Find(const std::string &id) override;

  void Add(const Course &) override { throw ReadOnlyRepositoryError(); }
  void Update(const Course &) override { throw ReadOnlyRepositoryError(); }
  void Remove(const std::string &) override { throw ReadOnlyRepositoryError(); }
  void ReplaceAll(const std::vector<Course> &) override {
    throw ReadOnlyRepositoryError();
  }

  std::vector<std::string> LoadCompleted() override { return {}; }
  void SetCompleted(const std::string &, bool) override {
    throw ReadOnlyRepositoryError();
  }

  bool IsReadOnly() const override { return true; }
  std::string Describe() const override { return "CSV (read-only): " + path_; }

private:
  std::string path_;
  std::optional<std::vector<Course>> cache_;

  const std::vector<Course> &load();
};

#endif // CSV_REPOSITORY_H
