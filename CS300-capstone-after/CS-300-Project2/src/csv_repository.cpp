#include "csv_repository.h"

#include <algorithm>
#include <filesystem>
#include <utility>

#include "csv.h"

CsvRepository::CsvRepository(std::string path) : path_(std::move(path)) {}

const std::vector<Course> &CsvRepository::load() {
  if (cache_)
    return *cache_;

  // readFile() quietly does nothing when the file can't be opened, so check
  // first and report it instead of loading an empty catalog.
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path_, ec))
    throw RepositoryError("Cannot open CSV file: " + path_);

  std::vector<std::string> lines;
  readFile(path_, lines);        // throws EmptyFileException
  Rows rows = FileToRows(lines); // throws the line-numbered CSV exceptions

  std::vector<Course> courses;
  courses.reserve(rows.size());
  for (const Row &row : rows) {
    std::vector<std::string> prereqs(row.begin() + 2, row.end());
    std::sort(prereqs.begin(), prereqs.end());
    // "CS200,DS,CS100,CS100" lists a prerequisite twice; keep it once.
    prereqs.erase(std::unique(prereqs.begin(), prereqs.end()), prereqs.end());
    courses.emplace_back(row[0], row[1], prereqs);
  }
  std::sort(
      courses.begin(), courses.end(),
      [](const Course &a, const Course &b) { return a.courseID < b.courseID; });

  cache_ = std::move(courses);
  return *cache_;
}

std::vector<Course> CsvRepository::LoadAll() { return load(); }

std::optional<Course> CsvRepository::Find(const std::string &id) {
  const auto &courses = load();
  auto it = std::lower_bound(
      courses.begin(), courses.end(), id,
      [](const Course &c, const std::string &key) { return c.courseID < key; });
  if (it != courses.end() && it->courseID == id)
    return *it;
  return std::nullopt;
}
