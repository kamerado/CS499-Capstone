/*
 * Presentation layer: menus and console I/O only.
 *
 * main() is the composition root. It is the one place that knows which
 * concrete repository is in use; it builds a CsvRepository or a
 * SqliteRepository, hands it to CourseService as an ICourseRepository&, and
 * from then on the menu talks only to the service.
 *
 *   CLI (this file) -> CourseService -> ICourseRepository
 *                                         ^              ^
 *                                  CsvRepository  SqliteRepository
 *
 * Usage:
 *   app                      ask for a CSV file or a database at startup
 *   app catalog.csv          open a CSV file (read-only)
 *   app catalog.db           open (or create) a SQLite database
 * Choosing "SQLite database" at the prompt and pressing Enter uses
 * <build folder>/catalog.db.
 */
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "Treeprint.h"
#include "course_service.h"
#include "csv.h" // trim()
#include "csv_repository.h"
#include "helpers.h"
#include "sqlite_repository.h"

namespace fs = std::filesystem;

// Where a new database goes when the user just presses Enter. CMake sets this
// to <build folder>/catalog.db; the fallback is the current folder.
#ifndef CATALOG_DEFAULT_DB
#define CATALOG_DEFAULT_DB "catalog.db"
#endif

namespace {

// ---- Console input
// ------------------------------------------------------------- std::getline
// (not cin >>) so names with spaces work. Returns false at end of input, so a
// closed stdin ends the program instead of looping forever.

bool prompt(const std::string &label, std::string &out) {
  std::cout << label;
  if (!std::getline(std::cin, out))
    return false;
  out = trim(out);
  return true;
}

std::vector<std::string> splitIds(const std::string &line) {
  std::vector<std::string> ids;
  std::stringstream ss(line);
  std::string part;
  while (std::getline(ss, part, ','))
    if (auto id = CourseService::NormalizeId(part); !id.empty())
      ids.push_back(id);
  return ids;
}

bool confirm(const std::string &question) {
  std::string answer;
  if (!prompt(question + " (y/n): ", answer))
    return false;
  return answer == "y" || answer == "Y" || answer == "yes";
}

bool isDatabasePath(const std::string &path) {
  auto ext = fs::path(path).extension().string();
  return ext == ".db" || ext == ".sqlite" || ext == ".sqlite3";
}

// ---- Choosing storage
// ------------------------------------------------------------

enum class StartOption : int { CsvFile = 1, Database = 2, Exit = 9 };

std::unique_ptr<ICourseRepository> openRepository(const std::string &path) {
  if (isDatabasePath(path))
    return std::make_unique<SqliteRepository>(path);
  if (!fs::exists(path))
    throw RepositoryError("File not found: " + path);
  return std::make_unique<CsvRepository>(path);
}

std::unique_ptr<ICourseRepository> chooseRepository() {
  while (true) {
    std::cout << "Where is the course catalog?\n"
              << "    1. CSV file (read-only)\n"
              << "    2. SQLite database (editable, created if missing)\n"
              << "    9. Exit\n\n";
    int choice = 0;
    if (!getInput(choice))
      return nullptr;

    std::string path;
    switch (static_cast<StartOption>(choice)) {
    case StartOption::CsvFile:
      if (!prompt("CSV file name: ", path))
        return nullptr;
      if (!fs::exists(path)) {
        std::cout << "File not found: " << path << "\n\n";
        continue;
      }
      return std::make_unique<CsvRepository>(path);
    case StartOption::Database:
      std::cout << "Database file (Enter for " << CATALOG_DEFAULT_DB << "): ";
      if (!prompt("", path))
        return nullptr;
      if (path.empty())
        path = CATALOG_DEFAULT_DB;
      try {
        return std::make_unique<SqliteRepository>(path);
      } catch (const std::exception &e) {
        std::cout << e.what() << "\n\n";
        continue;
      }
    case StartOption::Exit:
      return nullptr;
    default:
      std::cout << "Please enter a valid option.\n\n";
    }
  }
}

TreeKind chooseTree() {
  while (true) {
    std::cout << "Which search tree should hold the catalog in memory?\n"
              << "    1. AVL (self-balancing)\n"
              << "    2. BST (unbalanced, for comparison)\n\n";
    int choice = 0;
    if (!getInput(choice) || choice == 1)
      return TreeKind::AVL;
    if (choice == 2)
      return TreeKind::BST;
    std::cout << "Please enter a valid option.\n\n";
  }
}

// ---- Output
// ----------------------------------------------------------------------

void printCourse(const CourseService &service, const Course &c) {
  c.print();
  if (service.Completed().count(c.courseID))
    std::cout << "(completed)\n\n";
}

std::string treeFileName(const CourseService &service) {
  std::string base =
      service.Tree() == TreeKind::AVL ? "catalog_avl" : "catalog_bst";
  std::string name = base + ".svg";
  for (int i = 1; fs::exists(name); ++i)
    name = base + "(" + std::to_string(i) + ").svg";
  return name;
}

void writeTreeSvg(const CourseService &service) {
  std::string filename = treeFileName(service);
  std::ofstream svg(filename);
  if (service.Tree() == TreeKind::AVL)
    WriteSvg(service.Avl().Root(), svg);
  else
    WriteSvg(service.Bst().Root(), svg);
  std::cout << "Saved " << filename << "\n";
}

void printTree(const CourseService &service) {
  if (service.Tree() == TreeKind::AVL)
    PrintTree(service.Avl().Root(), std::cout);
  else
    PrintTree(service.Bst().Root(), std::cout);
}

// ---- Menu
// ------------------------------------------------------------------------

// Values match what the user types at the prompt.
enum class MenuOption : int {
  PrintCourseList = 1,
  PrintCourseTree = 2,
  PrintCourse = 3,
  GenerateTreeSvg = 4,
  PrintCoursePlan = 5,
  PrintAllPrereqs = 6,
  EligibleCourses = 7,
  SwitchTree = 8,
  Exit = 9,
  AddCourse = 10,
  UpdateCourse = 11,
  RemoveCourse = 12,
  MarkCompleted = 13,
  ImportCsv = 14,
};

void printMenu(const CourseService &service) {
  bool avl = service.Tree() == TreeKind::AVL;
  std::cout << "\n[" << service.StorageDescription() << " | "
            << (avl ? "AVL" : "BST") << " | " << service.CourseCount()
            << " courses]\n"
            << "    1. Print course list\n"
            << "    2. Print course tree\n"
            << "    3. Print course\n"
            << "    4. Save course tree as SVG\n"
            << "    5. Print course plan\n"
            << "    6. Print all prerequisites\n"
            << "    7. Courses I can take next\n"
            << "    8. Switch to " << (avl ? "BST" : "AVL") << "\n";
  if (service.CanEdit())
    std::cout << "   10. Add course\n"
              << "   11. Update course\n"
              << "   12. Remove course\n"
              << "   13. Mark course completed / not completed\n"
              << "   14. Import CSV file into database\n";
  std::cout << "    9. Exit\n\n";
}

// Each handler may throw; runMenu() reports the message and carries on.

void searchCourse(const CourseService &service) {
  std::string id;
  if (!prompt("Enter course ID: ", id))
    return;
  if (auto course = service.Find(id))
    printCourse(service, *course);
  else
    std::cout << "No course " << CourseService::NormalizeId(id) << ".\n";
}

void printPlan(const CourseService &service) {
  auto levels = service.Graph().Levels();
  for (std::size_t i = 0; i < levels.size(); ++i) {
    std::cout << "Term " << i + 1 << ":";
    for (const auto &id : levels[i])
      std::cout << ' ' << id;
    std::cout << '\n';
  }
}

void printAllPrereqs(const CourseService &service) {
  std::string id;
  if (!prompt("Enter course ID: ", id))
    return;
  id = CourseService::NormalizeId(id);
  if (!service.Graph().Contains(id)) {
    std::cout << "No course " << id << ".\n";
    return;
  }
  auto all = service.Graph().AllPrereqs(id);
  if (all.empty())
    std::cout << id << " has no prerequisites.\n";
  for (const auto &p : all)
    std::cout << p << '\n';
}

void printEligible(const CourseService &service) {
  auto courses = service.EligibleCourses();
  if (courses.empty()) {
    std::cout << "Nothing left to take.\n";
    return;
  }
  std::cout << "You can take these next:\n";
  for (const auto &c : courses)
    std::cout << "    " << c.courseID << "  " << c.courseTitle << '\n';
}

void addCourse(CourseService &service) {
  std::string id, title, prereqs;
  if (!prompt("Course ID: ", id) || !prompt("Title: ", title) ||
      !prompt("Prerequisites (comma-separated, blank for none): ", prereqs))
    return;
  service.AddCourse(Course(id, title, splitIds(prereqs)));
  std::cout << "Added " << CourseService::NormalizeId(id) << ".\n";
}

void updateCourse(CourseService &service) {
  std::string id;
  if (!prompt("Course ID to update: ", id))
    return;
  auto current = service.Find(id);
  if (!current)
    throw CourseNotFoundError(CourseService::NormalizeId(id));
  current->print();

  std::string title, prereqs;
  if (!prompt("New title (blank keeps it): ", title) ||
      !prompt("New prerequisites (comma-separated, blank keeps them, - for "
              "none): ",
              prereqs))
    return;
  Course updated = *current;
  if (!title.empty())
    updated.courseTitle = title;
  if (prereqs == "-")
    updated.prereq.clear();
  else if (!prereqs.empty())
    updated.prereq = splitIds(prereqs);

  service.UpdateCourse(updated);
  std::cout << "Updated " << updated.courseID << ".\n";
}

void removeCourse(CourseService &service) {
  std::string id;
  if (!prompt("Course ID to remove: ", id))
    return;
  id = CourseService::NormalizeId(id);
  if (!confirm("Permanently remove " + id + "?"))
    return;
  service.RemoveCourse(id);
  std::cout << "Removed " << id << ".\n";
}

void markCompleted(CourseService &service) {
  std::string id;
  if (!prompt("Course ID: ", id))
    return;
  id = CourseService::NormalizeId(id);
  bool nowCompleted = !service.Completed().count(id);
  service.MarkCompleted(id, nowCompleted);
  std::cout << id
            << (nowCompleted ? " marked completed.\n"
                             : " marked not completed.\n");
}

void importCsv(CourseService &service) {
  std::string path;
  if (!prompt("CSV file to import: ", path) || path.empty())
    return;
  if (!fs::exists(path))
    throw RepositoryError("File not found: " + path);
  if (service.CourseCount() > 0 &&
      !confirm("This replaces all " + std::to_string(service.CourseCount()) +
               " courses and your completed list. Continue?"))
    return;
  CsvRepository source(path);
  std::size_t n = service.ImportFrom(source);
  std::cout << "Imported " << n << " courses.\n";
}

// Returns false when the user chooses Exit.
bool handle(CourseService &service, int input) {
  auto option = static_cast<MenuOption>(input);
  bool editOption = input >= static_cast<int>(MenuOption::AddCourse);
  if (editOption && !service.CanEdit()) {
    std::cout << "Please input a valid option.\n";
    return true;
  }

  switch (option) {
  case MenuOption::PrintCourseList:
    for (const auto &c : service.AllCoursesSorted())
      printCourse(service, c);
    break;
  case MenuOption::PrintCourseTree:
    printTree(service);
    break;
  case MenuOption::PrintCourse:
    searchCourse(service);
    break;
  case MenuOption::GenerateTreeSvg:
    writeTreeSvg(service);
    break;
  case MenuOption::PrintCoursePlan:
    printPlan(service);
    break;
  case MenuOption::PrintAllPrereqs:
    printAllPrereqs(service);
    break;
  case MenuOption::EligibleCourses:
    printEligible(service);
    break;
  case MenuOption::SwitchTree:
    service.SetTree(service.Tree() == TreeKind::AVL ? TreeKind::BST
                                                    : TreeKind::AVL);
    break;
  case MenuOption::AddCourse:
    addCourse(service);
    break;
  case MenuOption::UpdateCourse:
    updateCourse(service);
    break;
  case MenuOption::RemoveCourse:
    removeCourse(service);
    break;
  case MenuOption::MarkCompleted:
    markCompleted(service);
    break;
  case MenuOption::ImportCsv:
    importCsv(service);
    break;
  case MenuOption::Exit:
    return false;
  default:
    std::cout << "Please input a valid option.\n";
  }
  return true;
}

void runMenu(CourseService &service) {
  while (true) {
    printMenu(service);
    int input = 0;
    if (!getInput(input))
      return; // end of input
    try {
      if (!handle(service, input))
        return;
    } catch (const std::exception &e) {
      // Nothing was changed: the service validates first and the
      // repository writes all-or-nothing.
      std::cout << "Error: " << e.what() << '\n';
    }
  }
}

} // namespace

int main(int argc, char *argv[]) {
  std::cout << "Welcome to the ABCU course planner.\n\n";

  std::unique_ptr<ICourseRepository> repo;
  try {
    repo = argc > 1 ? openRepository(argv[1]) : chooseRepository();
  } catch (const std::exception &e) {
    std::cout << "Error: " << e.what() << '\n';
    return 1;
  }
  if (!repo) {
    std::cout << "Goodbye.\n";
    return 0;
  }

  CourseService service(*repo, chooseTree());
  try {
    service.Load();
  } catch (const std::exception &e) {
    std::cout << "Could not load the catalog: " << e.what() << '\n';
    return 1;
  }

  if (service.CanEdit() && service.CourseCount() == 0) {
    std::cout << "The database is empty.\n";
    try {
      importCsv(service);
    } catch (const std::exception &e) {
      std::cout << "Error: " << e.what() << '\n';
    }
  }

  runMenu(service);
  std::cout << "Goodbye.\n";
  return 0;
}
