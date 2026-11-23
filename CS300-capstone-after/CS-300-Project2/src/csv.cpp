#include <algorithm>
#include <fstream>
#include <unordered_set>

#include "csv.h"

/*
 * helper function to read file into vector.
 */
void readFile(std::string csvPath, std::vector<std::string> &_originalFile) {
  std::string line;

  std::ifstream ifile(csvPath.c_str());
  if (ifile.is_open()) {
    while (std::getline(ifile, line)) {
      if (line != "") {
        _originalFile.push_back(line);
      }
    }
    ifile.close();

    if (_originalFile.size() == 0) { // If no data, exit program.
      throw EmptyFileException();
    }
  }
}

/*
 * helper function to trim whitespace, \t\r\n.
 */
std::string trim(const std::string &s) {
  const char *ws = " \t\r\n";
  std::size_t start = s.find_first_not_of(ws);
  if (start == std::string::npos) {
    return ""; // all whitespace
  }
  std::size_t end = s.find_last_not_of(ws);
  return s.substr(start, end - start + 1);
}

bool verifyFile(const Rows &rows) {
  std::unordered_set<std::string> ids;
  for (const auto &row : rows) {
    if (!ids.insert(row[0]).second) {
      // Duplicate courseID, fail.
      throw CSVDuplicateCourseException(row[0]);
    }
  }

  for (const Row &row : rows) {
    for (std::size_t i = 2; i < row.size(); i++) {
      if (ids.count(row[i]) == 0) {
        // Prerequisite is not a course.
        throw CSVPrerequisiteNotCourseException(row[i], row[0]);
      }
    }
  }

  return true;
}

/*
 * helper function to read from vector into rows with individual columns.
 */
Rows FileToRows(std::vector<std::string> &_originalFile) {
  Rows rows = Rows();

  // Iterate through each line in file contents.
  std::vector<std::string>::iterator it;
  it = _originalFile.begin();
  int lineNum = 0;

  for (; it != _originalFile.end(); it++) { // For every line.
    int tokenStart = 0;
    unsigned int i = 0;

    lineNum++;

    // Skip if empty.
    if (!trim(*it).empty()) {
      Row row = Row();
      for (; i < it->length();
           i++) { // Parse every char, if ',' then push substr into Row.
        if (it->at(i) == ',') {
          row.push_back(trim(it->substr(tokenStart, i - tokenStart)));
          tokenStart = i + 1; // Update start pos.
        }
      }

      row.push_back(
          trim(it->substr(tokenStart,
                          it->length() - tokenStart))); // Push last section.

      if (row.size() < 2) { // if Row is less than two params error.
        throw CSVInvalidFormatException(lineNum);
      }

      // Remove empty prerequisite fields (trailing or doubled commas).
      row.erase(std::remove_if(row.begin() + 2, row.end(),
                               [](const std::string &s) { return s.empty(); }),
                row.end());

      if (row[0].empty()) {
        throw CSVMissingCourseIDException(lineNum);
      }
      if (row[1].empty()) { // Ensure title and courseID are present
        throw CSVMissingCourseTitleException(lineNum);
      }

      // Ensure that the course does not list itself as prerequisite.
      for (std::size_t i = 2; i < row.size(); i++) {
        if (row[0] == row[i]) {
          throw CSVSelfIsPrereqException(lineNum);
        }
      }
      rows.push_back(row);
    }
  }

  verifyFile(rows);
  return rows;
}
