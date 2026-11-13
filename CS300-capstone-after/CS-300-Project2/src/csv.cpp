#include <algorithm>
#include <fstream>
#include <iostream>
#include <unordered_set>

#include "csv.h"

/*
 * helper function to read file into vector.
 */
// FIXED: (SE)[Error Handling]: If the file fails to open, nothing is reported
// and laoding silently does nothing. Report the failure to the caller.
// FIXED: (SE)[Correctness]: While (ifile.good()) + getline is fragile. Use
// while (getline()) instead. FIXED: (SE)[Error Handling]: exit(1) inside the
// helper kills the program and skips cleanup. Throw or return an error so main
// cn decide what to do.
void readFile(std::string csvPath, std::vector<std::string> &_originalFile) {
  std::string line;

  std::ifstream ifile(csvPath.c_str());
  try {
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
  } catch (const EmptyFileException &e) {
    std::cout << "Error: " << e.what() << std::endl;
  } catch (const std::exception &) {
    std::cout << "An error occored when opening file." << std::endl;
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
// FIXED: (SE)[Memory]: Allocating new row() each loop but never frees them.
// Since we are pushing back row into rows, it copies it, and we empty the
// origonal. We never get rid of that original object and after the loop ends,
// we lose the ability to free and it never is freed.
// Fix: We dont need to allocte here at all. We can just let the containers
// own the data so whent he loop ends, leftover data is destoryed
// automatically. Simplest fix.
// FIXED: (SE)[Memory]: There is no input validation here, no whitespace
// trimming, \r trimming, for windows, this would likely break.
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
