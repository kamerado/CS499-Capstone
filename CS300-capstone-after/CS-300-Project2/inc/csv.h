#ifndef CSV_H
#define CSV_H

#include <exception>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

class EmptyFileException : public std::exception {
public:
  const char *what() const noexcept override { return "File is empty."; }
};

class CSVException : public std::runtime_error {
public:
  using std::runtime_error::runtime_error; // inherit the string constructor
};

class CSVInvalidFormatException : public CSVException {
public:
  explicit CSVInvalidFormatException(int line)
      : CSVException(std::format(
            "CSV line: {} invalid format: less than two columns", line)) {}
};

class CSVMissingCourseIDException : public CSVException {
public:
  explicit CSVMissingCourseIDException(int line)
      : CSVException(std::format("CSV line: {} is missing ID field.", line)) {}
};

class CSVMissingCourseTitleException : public CSVException {
public:
  explicit CSVMissingCourseTitleException(int line)
      : CSVException(
            std::format("CSV line: {} is missing Title field.", line)) {}
};

class CSVSelfIsPrereqException : public CSVException {
public:
  explicit CSVSelfIsPrereqException(int line)
      : CSVException(std::format(
            "CSV line: {} course lists itself as a prerequisite.", line)) {}
};

class CSVDuplicateCourseException : public CSVException {
public:
  explicit CSVDuplicateCourseException(const std::string &courseID)
      : CSVException(std::format("{} is duplicate course.", courseID)) {}
};

class CSVPrerequisiteNotCourseException : public CSVException {
public:
  explicit CSVPrerequisiteNotCourseException(const std::string &prereq,
                                             const std::string &courseID)
      : CSVException(std::format("{} lists unknown prerequisite {}.", courseID,
                                 prereq)) {}
};

typedef std::vector<std::string> Row;
typedef std::vector<Row> Rows;

void readFile(std::string csvPath, std::vector<std::string> &_originalFile);
std::string trim(const std::string &s);
bool verifyFile(const Rows &rows);
Rows FileToRows(std::vector<std::string> &_originalFile);

#endif
