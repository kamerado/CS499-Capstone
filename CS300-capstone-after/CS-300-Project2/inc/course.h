#ifndef COURSE_H
#define COURSE_H

#include <iostream>
#include <string>
#include <vector>

// Struct course.
struct Course {
  std::string courseID;
  std::string courseTitle;
  std::vector<std::string> prereq;

  // default constructor.
  Course() = default;

  // FIXed: (SE)[Structure]: Constructors take strings and vectors by value. use
  // const&.
  // Default constructor assigning "\0" just makes an expty string. use default
  // member initializers instead. constructor with id.
  explicit Course(const std::string &id) : courseID(id) {}

  // cosntructor with id, title.
  Course(const std::string &id, const std::string &title)
      : courseID(id), courseTitle(title) {}

  // constructor with id, title, prereqs.
  Course(const std::string &id, const std::string &title,
         const std::vector<std::string> &preReq)
      : courseID(id), courseTitle(title), prereq(preReq) {}

  // Func to print course data.
  void print() const {
    std::cout << "Course ID: " << courseID << "\n"
              << "Course Title: " << courseTitle << "\n";
    if (!prereq.empty()) {
      auto it = prereq.begin();
      std::cout << "Prerequisites: " << "\n";
      for (; it < prereq.end(); it++) {
        std::cout << "    " << *it << "\n";
      }
      std::cout << std::endl;
    }
  }

  const std::string &getKey() const { return courseID; }
};

#endif // !COURSE_H
#define COURSE_H
