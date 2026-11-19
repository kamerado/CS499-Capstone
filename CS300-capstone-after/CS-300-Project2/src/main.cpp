#include <algorithm>
#include <cctype>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "avl.h"
#include "bst.h"
#include "course.h"
#include "csv.h"
#include "helpers.h"

/*
 * helper fucntion to insert into BST from rows.
 */
// TODO: (DB)[Data integrity]: Prerequisites are never checked against
// courses, dupe course IDS are accepted. In SQLite A PRIMARY KEY on
// courseID and FOREIGN KEY on Prerequisites enforce both.
template <typename T> void RowsToDS(T &DS, Rows rows) {
  /*
   * iterate over rows.
   */
  Rows::iterator it2 = rows.begin();

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
    DS.Insert(course); // add Course to BST<T> where T = Course.
  }
}

/*
 * function to parse csv file.
 */
// TODO: (DB)[Structure]: Replace in-memory-only storage with SQLite.
// courses(course_id PK, title) and prerequisites(course_id, prereq_id) with
// FKs. Import the CSV inside a transaction using parameterized queries only.
template <typename T>
void csvParse(T &DS, std::string &csvpath,
              std::vector<std::string> &_originalFile) {
  // read csv file into vector of lines.
  readFile(csvpath, _originalFile);

  // Read contents to rows.
  try {
    Rows rows = FileToRows(_originalFile);
    // insert rows into bst.
    RowsToDS(DS, rows);
  } catch (const std::exception &e) {
    std::cout << "Could not load file: " << e.what() << std::endl;
  }
}

/*
 * helper function to get user input and search for input in BST.
 */
// TODO: (DB): Search will become a parameterized SELECT once SQLite is added.
template <typename T> void search(T &DS) {
  clear();

  std::string key;
  std::cout << "Enter courseID: ";
  getInput(key);

  std::transform(key.begin(), key.end(), key.begin(),
                 [](unsigned char c) { return std::toupper(c); });

  clear();

  auto tmp = DS.Search(key);
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
void printMenu(int avlOrbst) {
  // clear();
  if (avlOrbst == 1)
    std::cout << "\n\n    1. Load File Into AVL.\n";
  else if (avlOrbst == 2) {
    std::cout << "\n\n    1. Load File Into BST.\n";
  }
  std::cout << "    2. Print Course List.\n";
  std::cout << "    3. Print Course Tree.\n";
  std::cout << "    4. Print Course.\n";
  if (avlOrbst == 1)
    std::cout << "    8. Switch to bst.\n";
  else if (avlOrbst == 2) {
    std::cout << "    8. Switch to avl.\n";
  }
  std::cout << "    9. Exit.\n\n";
}

// Menu options. Values match what the user types at the prompt.
enum class MenuOption : int {
  LoadFile = 1,
  PrintCourseList = 2,
  PrintCourseTree = 3,
  PrintCourse = 4,
  Switch = 8,
  Exit = 9
};
//
// Menu options. Values match what the user types at the prompt.
enum class DSOption : int { AVLTree = 1, BST = 2, Exit = 9 };

/*
 * helper fucntion to decide program logic based on user input.
 */
template <typename T>
void handleInput(T &DS, int &input, bool &loop, int &avlOrbst,
                 std::string &csvpath,
                 std::vector<std::string> &_originalFile) {
  switch (static_cast<MenuOption>(input)) {
  case MenuOption::LoadFile:
    DS.Clear();
    _originalFile.clear();
    csvParse(DS, csvpath, _originalFile);
    clear();
    break;
  case MenuOption::PrintCourseList:
    clear();
    DS.InOrder([](const Course &c) { c.print(); });
    break;
  case MenuOption::PrintCourseTree:
    clear();
    std::cout << "Not implemented yet." << std::endl;
    break;
  case MenuOption::PrintCourse:
    search(DS);
    break;
  case MenuOption::Switch:
    avlOrbst = (avlOrbst == 1) ? 2 : 1;
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

int avlOrBst() {
  int input;
  std::cout << "Would you like to use avl or bst?\n";
  while (true) {
    std::cout << "1. AVL\n";
    std::cout << "2. BST\n";
    std::cout << "9. Exit" << std::endl;
    getInput(input);
    if (input == 1 || input == 2 || input == 9)
      return input;
    std::cout << "Please enter a valid option.";
  }
}

/*
 * main fucntion.
 */
int main() {
  // Declare variables for functon implementaions.
  BinarySearchTree<Course> bst;
  AVLTree<Course> avl;
  std::vector<std::string> _originalFile;
  std::string csvpath;
  bool loop = true;
  int input;
  int avlOrbst;

  getPath(csvpath);
  clear();

  std::cout << "\nWelcome to the course planner." << std::endl;
  avlOrbst = avlOrBst();
  while (loop) {
    printMenu(avlOrbst);
    if (getInput(input)) {
      switch (static_cast<DSOption>(avlOrbst)) {
      case DSOption::AVLTree:
        handleInput(avl, input, loop, avlOrbst, csvpath, _originalFile);
        break;
      case DSOption::BST:
        handleInput(bst, input, loop, avlOrbst, csvpath, _originalFile);
        break;
      case DSOption::Exit:
        loop = false;
        break;
      default:
        clear();
        std::cout << "Please input a valid option." << std::endl;
        break;
      }
    }
  }

  clear();
  std::cout << "Goodbye." << std::endl;
}
