#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Treeprint.h"
#include "avl.h"
#include "bst.h"
#include "course.h"
#include "csv.h"
#include "helpers.h"

struct App {
  BinarySearchTree<Course> bst;
  AVLTree<Course> avl;
  std::string csvpath;
  std::vector<std::string> originalFile;
  int mode = 1; // 1 = AVL, 2 = BST
  bool running = true;
};

std::string treeFileName(const App &app) {
  namespace fs = std::filesystem;

  std::string base = fs::path(app.csvpath).stem().string() + "_" +
                     (app.mode == 1 ? "avl" : "bst");

  std::string name = base + ".svg";
  for (int i = 1; fs::exists(name); ++i)
    name = base + "(" + std::to_string(i) + ").svg";

  return name;
}

template <typename T> void GenerateTreePng(App &app, T &DS) {
  std::string filename = treeFileName(app);
  std::ofstream svg(filename);
  WriteSvg(DS.Root(), svg);
  std::cout << "Saved" << filename << std::endl;
}

/*
 * helper fucntion to insert into BST from rows.
 */
// TODO: (DB)[Data integrity]: Prerequisites are never checked against
// courses, dupe course IDS are accepted. In SQLite A PRIMARY KEY on
// courseID and FOREIGN KEY on Prerequisites enforce both.
template <typename T> void RowsToDS(T &DS, Rows &rows) {
  /*
   * iterate over rows.
   */
  Rows::iterator it2 = rows.begin();

  for (; it2 != rows.end(); it2++) { // for each row.

    // Get data.
    std::string courseid = it2->at(0);
    std::string coursetitle = it2->at(1);
    std::vector<std::string> prereqs;
    for (std::size_t i = 2; i < it2->size(); i++) {
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
template <typename T> void csvParse(App &app, T &DS) {
  // Read contents to rows.
  try {
    Rows rows = FileToRows(app.originalFile);
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
void getPath(App &app) {
  std::cout << "\nPlease enter csv file name: " << std::endl;
  std::string csvpath;
  getInput(csvpath);
  app.csvpath = csvpath;

  while (!exists(csvpath)) {
    clear();
    std::cout << "\nFilename invalid." << std::endl;

    std::cout << "\nPlease enter csv file name: " << std::endl;
    getInput(csvpath);
  }
  try {
    readFile(csvpath, app.originalFile);
  } catch (const EmptyFileException &e) {
    std::cout << "Error: " << e.what() << std::endl;
  } catch (const std::exception &) {
    std::cout << "An error occored when opening file." << std::endl;
  }
}

/*
 * helper function to print menu.
 */
// TODO: (DB)[Structure]: Add menu options to add, update, and delete courses.
void printMenu(int avlOrbst) {
  std::cout << "    1. Print Course List.\n";
  std::cout << "    2. Print Course Tree.\n";
  std::cout << "    3. Print Course.\n";
  std::cout << "    4. Generate Tree PNG.\n";
  if (avlOrbst == 1)
    std::cout << "    8. Switch to bst.\n";
  else if (avlOrbst == 2) {
    std::cout << "    8. Switch to avl.\n";
  }
  std::cout << "    9. Exit.\n\n";
}

// Menu options. Values match what the user types at the prompt.
enum class MenuOption : int {
  PrintCourseList = 1,
  PrintCourseTree = 2,
  PrintCourse = 3,
  GenerateTreePNG = 4,
  Switch = 8,
  Exit = 9
};
//
// Menu options. Values match what the user types at the prompt.
enum class DSOption : int { AVLTree = 1, BST = 2, Exit = 9 };

void loadIntoDS(App &app) {
  if (app.mode == 1)
    csvParse(app, app.avl);
  else if (app.mode == 2)
    csvParse(app, app.bst);
}

/*
 * helper fucntion to decide program logic based on user input.
 */
template <typename T> void handleInput(App &app, T &DS, int &input) {
  switch (static_cast<MenuOption>(input)) {
  case MenuOption::PrintCourseList:
    clear();
    DS.InOrder([](const Course &c) { c.print(); });
    break;
  case MenuOption::PrintCourseTree:
    PrintTree(DS.Root(), std::cout);
    break;
  case MenuOption::PrintCourse:
    search(DS);
    break;
  case MenuOption::GenerateTreePNG:
    GenerateTreePng(app, DS);
    break;
  case MenuOption::Switch:
    app.mode = (app.mode == 1) ? 2 : 1;
    DS.Clear();
    loadIntoDS(app);
    break;
  case MenuOption::Exit:
    app.running = false;
    break;
  default:
    clear();
    std::cout << "Please input a valid option." << std::endl;
    break;
  }
}

void avlOrBst(App &app) {
  int input;

  clear();
  std::cout << "Would you like to use avl or bst?\n";

  while (true) {
    std::cout << "1. AVL\n";
    std::cout << "2. BST\n";
    std::cout << "9. Exit" << std::endl;
    getInput(input);
    clear();
    if (input == 1) {
      csvParse(app, app.avl);
      app.mode = input;
      return;
    } else if (input == 2) {
      csvParse(app, app.bst);
      app.mode = input;
      return;
    }

    std::cout << "Please enter a valid option.";
  }
}

/*
 * main fucntion.
 */
int main() {
  // Declare variables for functon implementaions.
  App app;
  int input;

  getPath(app);
  avlOrBst(app);

  std::cout << "\nWelcome to the course planner." << std::endl;
  while (app.running) {
    printMenu(app.mode);
    if (getInput(input)) {
      if (app.mode == 1)
        handleInput(app, app.avl, input);
      else
        handleInput(app, app.bst, input);
    }
  }

  clear();
  std::cout << "Goodbye." << std::endl;
}
