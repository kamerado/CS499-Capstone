#include <algorithm>
#include <cctype>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "bst.h"
#include "course.h"
#include "csv.h"
#include "helpers.h"

/*
 * helper fucntion to insert into BST from rows.
 */
// TODO: (DB)[Data integrity]: Prerequisites are never checked against
// courses, dupe course IDS are accepted. In SQLite A PRIMARY KEY on courseID
// and FOREIGN KEY on Prerequisites enforce both.
void RowsToBST(BinarySearchTree<Course> &bst, Rows rows) {
  /*
   * iterate over rows.
   */
  Rows::iterator it2 = rows.begin();

  int n = 0;                         // unused
  for (; it2 != rows.end(); it2++) { // for each row.

    // Get data.
    std::string courseid = it2->at(0);
    std::string coursetitle = it2->at(1);
    std::vector<std::string> prereqs;
    for (int i = 2; i < it2->size(); i++) {
      prereqs.push_back(it2->at(i));
    }

    // add data to Course.
    Course course(courseid, coursetitle, prereqs);
    bst.Insert(course); // add Course to BST<T> where T = Course.
  }
}

/*
 * function to parse csv file.
 */
// TODO: (SE)[Memory]: Rows* from FileToRows is never deleted here.
// TODO: (DB)[Structure]: Replace in-memory-only storage with SQLite.
// courses(course_id PK, title) and prerequisites(course_id, prereq_id) with
// FKs. Import the CSV inside a transaction using parameterized queries only.
void csvParse(BinarySearchTree<Course> &bst, std::string &csvpath,
              std::vector<std::string> &_originalFile) {
  // read csv file into vector of lines.
  readFile(csvpath, _originalFile);

  // Read contents to rows.
  try {
    Rows rows = FileToRows(_originalFile);
    // insert rows into bst.
    RowsToBST(bst, rows);
  } catch (const std::exception &e) {
    std::cout << "Could not load file: " << e.what() << std::endl;
  }
}

/*
 * helper function to get user input and search for input in BST.
 */
// TODO: (DB): Search will become a parameterized SELECT once SQLite is added.
void search(BinarySearchTree<Course> &bst) {
  clear();

  std::string key;
  std::cout << "Enter courseID: ";
  getInput(key);

  std::transform(key.begin(), key.end(), key.begin(),
                 [](unsigned char c) { return std::toupper(c); });

  clear();

  auto tmp = bst.Search(key);
  if (tmp.has_value() && tmp.value().getKey() == key) {
    tmp.value().print();
  } else {
    std::cout << "No valid search results." << std::endl;
  }
}

/*
 * function to print out the custom item type.
 */
void print(Node<Course> *node) { node->item.print(); }

/*
 * helper fucntion to get file name as input from user.
 */
void getPath(std::string &csvpath) {
  std::cout << "\nPlease enter csv file name: " << std::endl;
  getInput(csvpath);

  while (!exists(csvpath)) {
    clear();
    std::cout << "\nFilename invalid." << std::endl;

    std::cout << "\nPlease enter csv file name: " << std::endl;
    getInput(csvpath);
  }
}

/*
 * helper function to print menu.
 */
// TODO: (DB)[Structure]: Add menu options to add, update, and delete courses.
void printMenu() {
  std::cout << "\n\n    1. Load File Into BST.\n";
  std::cout << "    2. Print Course List.\n";
  std::cout << "    3. Print Course.\n";
  std::cout << "    9. Exit.\n\n";
}

// Menu options. Values match what the user types at the prompt.
enum class MenuOption : int {
  LoadFile = 1,
  PrintCourseList = 2,
  PrintCourse = 3,
  Exit = 9
};

/*
 * helper fucntion to decide program logic based on user input.
 */
// FIXED: (SE)[Variables]: csvpath and _origonalFile are passed by value, so
// main's _oringoinalFile is never filled and is pointless.
// FIXED: (SE)[Structure]: Menu choices are 1, 2, 3, 9, which are magic
// numbers, replace with enum class menuOption.
void handleInput(BinarySearchTree<Course> &bst, int &input, bool &loop,
                 std::string &csvpath,
                 std::vector<std::string> &_originalFile) {
  switch (static_cast<MenuOption>(input)) {
  case MenuOption::LoadFile:
    csvParse(bst, csvpath, _originalFile);
    clear();
    break;
  case MenuOption::PrintCourseList:
    clear();
    bst.InOrder([](const Course &c) { c.print(); });
    break;
  case MenuOption::PrintCourse:
    search(bst);
    break;
  case MenuOption::Exit:
    loop = false;
    break;
  default:
    clear();
    std::cout << "Please input a valid option." << std::endl;
    break;
  }
}

/*
 * main fucntion.
 */
int main() {
  // Declare variables for functon implementaions.
  BinarySearchTree<Course> bst;
  std::vector<std::string> _originalFile;
  std::string csvpath;
  bool loop = true;
  int input;

  getPath(csvpath);
  clear();

  std::cout << "\nWelcome to the course planner." << std::endl;
  while (loop) {
    printMenu();
    if (getInput(input)) {
      handleInput(bst, input, loop, csvpath, _originalFile);
    }
  }

  clear();
  std::cout << "Goodbye." << std::endl;
}
