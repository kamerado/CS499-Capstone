// Tests for CourseService, the business-logic layer.
//
// Most tests use InMemoryRepository (tests/fake_repository.h), so they check
// the service's own logic with no file or database involved. That is the
// point of the ICourseRepository interface: the service can be tested on its
// own. A few tests at the end run it over the real CSV and SQLite backends.

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "course_service.h"
#include "csv_repository.h"
#include "fake_repository.h"
#include "graph.h"
#include "sqlite_repository.h"

// Readable names in ctest output instead of "4-byte object <00-00 00-00>".
inline void PrintTo(TreeKind kind, std::ostream *os) {
  *os << (kind == TreeKind::AVL ? "AVL" : "BST");
}

namespace {

using Ids = std::vector<std::string>;

Ids ids(const std::vector<Course> &courses) {
  Ids out;
  for (const auto &c : courses)
    out.push_back(c.courseID);
  return out;
}

const std::vector<Course> kSample = {
    {"CSCI100", "Introduction to Computer Science", {}},
    {"CSCI101", "Introduction to Programming in C++", {"CSCI100"}},
    {"CSCI200", "Data Structures", {"CSCI101"}},
    {"CSCI300", "Introduction to Algorithms", {"CSCI200", "MATH201"}},
    {"CSCI301", "Advanced Programming in C++", {"CSCI101"}},
    {"CSCI350", "Operating Systems", {"CSCI300"}},
    {"CSCI400", "Large Software Development", {"CSCI301", "CSCI350"}},
    {"MATH201", "Discrete Mathematics", {}},
};

const Ids kSampleIds = {"CSCI100", "CSCI101", "CSCI200", "CSCI300",
                        "CSCI301", "CSCI350", "CSCI400", "MATH201"};

class ServiceTest : public ::testing::TestWithParam<TreeKind> {
protected:
  void SetUp() override {
    repo.ReplaceAll(kSample);
    repo.writes = 0;
    service.Load();
  }
  InMemoryRepository repo;
  CourseService service{repo, GetParam()};
};

// ---- Loading and queries ---------------------------------------------------

TEST_P(ServiceTest, LoadBuildsTreeAndGraph) {
  EXPECT_TRUE(service.Loaded());
  EXPECT_EQ(ids(service.AllCoursesSorted()), kSampleIds);
  EXPECT_EQ(service.Graph().CourseCount(), 8u);
  EXPECT_EQ(service.Graph().EdgeCount(), 8u);
}

TEST_P(ServiceTest, FindNormalizesTheId) {
  auto c = service.Find("  csci300 ");
  ASSERT_TRUE(c.has_value());
  EXPECT_EQ(c->courseTitle, "Introduction to Algorithms");
  EXPECT_FALSE(service.Find("CSCI999").has_value());
}

TEST_P(ServiceTest, ReadsDoNotTouchStorage) {
  service.Find("CSCI100");
  service.AllCoursesSorted();
  service.EligibleCourses();
  EXPECT_EQ(repo.writes, 0);
}

TEST_P(ServiceTest, ActiveTreeHoldsTheCatalog) {
  if (GetParam() == TreeKind::AVL) {
    EXPECT_NE(service.Avl().Root(), nullptr);
    EXPECT_EQ(service.Bst().Root(), nullptr);
  } else {
    EXPECT_NE(service.Bst().Root(), nullptr);
    EXPECT_EQ(service.Avl().Root(), nullptr);
  }
}

TEST_P(ServiceTest, SwitchingTreesKeepsTheCatalog) {
  TreeKind other = GetParam() == TreeKind::AVL ? TreeKind::BST : TreeKind::AVL;
  service.SetTree(other);
  EXPECT_EQ(service.Tree(), other);
  EXPECT_EQ(ids(service.AllCoursesSorted()), kSampleIds);
  EXPECT_TRUE(service.Find("CSCI400").has_value());
}

// ---- Adding ----------------------------------------------------------------

TEST_P(ServiceTest, AddWritesToStorageAndCache) {
  service.AddCourse({" csci500 ", " Capstone ", {"csci400", "MATH201"}});

  auto stored = repo.Find("CSCI500");
  ASSERT_TRUE(stored.has_value());
  EXPECT_EQ(stored->courseTitle, "Capstone");
  EXPECT_EQ(stored->prereq, (Ids{"CSCI400", "MATH201"}));

  EXPECT_TRUE(service.Find("CSCI500").has_value());
  EXPECT_EQ(service.Graph().DirectPrereqs("CSCI500"),
            (Ids{"CSCI400", "MATH201"}));
}

TEST_P(ServiceTest, InvalidAddNeverReachesStorage) {
  EXPECT_THROW(service.AddCourse({"CSCI500", "X", {"NOPE999"}}),
               UnknownCourseError);
  EXPECT_THROW(service.AddCourse({"CSCI100", "Dup", {}}), DuplicateCourseError);
  EXPECT_THROW(service.AddCourse({"CSCI500", "X", {"csci500"}}),
               SelfPrerequisiteError);
  EXPECT_THROW(service.AddCourse({"", "No ID", {}}), InvalidCourseError);
  EXPECT_THROW(service.AddCourse({"CSCI500", "   ", {}}), InvalidCourseError);
  EXPECT_EQ(repo.writes, 0);
  EXPECT_EQ(service.CourseCount(), 8u);
}

TEST_P(ServiceTest, StorageFailureLeavesCacheUnchanged) {
  repo.failNextWrite = true;
  EXPECT_THROW(service.AddCourse({"CSCI500", "Capstone", {}}), RepositoryError);
  EXPECT_FALSE(service.Find("CSCI500").has_value());
  EXPECT_FALSE(service.Graph().Contains("CSCI500"));
}

// ---- Updating --------------------------------------------------------------

TEST_P(ServiceTest, UpdateChangesStorageAndCache) {
  service.UpdateCourse({"csci300", "Algorithms", {"CSCI200"}});
  EXPECT_EQ(repo.Find("CSCI300")->prereq, Ids{"CSCI200"});
  EXPECT_EQ(service.Find("CSCI300")->courseTitle, "Algorithms");
  EXPECT_EQ(service.Graph().DirectPrereqs("CSCI300"), Ids{"CSCI200"});
}

TEST_P(ServiceTest, UpdateThatCreatesACycleIsRejected) {
  // CSCI100 -> CSCI101 -> CSCI200 already; making CSCI100 need CSCI200
  // closes a loop. SQLite constraints can't see this; the service must.
  EXPECT_THROW(service.UpdateCourse({"CSCI100", "Intro", {"CSCI200"}}),
               CyclicPrereqException);
  EXPECT_EQ(repo.writes, 0);
  EXPECT_TRUE(repo.Find("CSCI100")->prereq.empty());
  EXPECT_TRUE(service.Find("CSCI100")->prereq.empty());
  EXPECT_FALSE(service.Graph().HasCycle());
}

TEST_P(ServiceTest, UpdateMissingCourseThrows) {
  EXPECT_THROW(service.UpdateCourse({"CSCI999", "X", {}}), CourseNotFoundError);
}

// ---- Removing --------------------------------------------------------------

TEST_P(ServiceTest, RemoveLeafCourse) {
  service.RemoveCourse("csci400");
  EXPECT_FALSE(repo.Find("CSCI400").has_value());
  EXPECT_FALSE(service.Find("CSCI400").has_value());
  EXPECT_FALSE(service.Graph().Contains("CSCI400"));
}

TEST_P(ServiceTest, RemoveCourseInUseIsRejectedBeforeStorage) {
  EXPECT_THROW(service.RemoveCourse("CSCI101"), CourseInUseError);
  EXPECT_EQ(repo.writes, 0);
  EXPECT_TRUE(service.Find("CSCI101").has_value());
}

TEST_P(ServiceTest, RemoveMissingCourseThrows) {
  EXPECT_THROW(service.RemoveCourse("CSCI999"), CourseNotFoundError);
}

// ---- Completed and eligible courses ----------------------------------------

TEST_P(ServiceTest, EligibleCoursesFollowCompletion) {
  EXPECT_EQ(ids(service.EligibleCourses()), (Ids{"CSCI100", "MATH201"}));

  service.MarkCompleted("csci100");
  EXPECT_EQ(repo.LoadCompleted(), Ids{"CSCI100"});
  EXPECT_EQ(ids(service.EligibleCourses()), (Ids{"CSCI101", "MATH201"}));

  service.MarkCompleted("CSCI100", false);
  EXPECT_EQ(ids(service.EligibleCourses()), (Ids{"CSCI100", "MATH201"}));
}

TEST_P(ServiceTest, CompletedCoursesAreReloaded) {
  service.MarkCompleted("CSCI100");
  CourseService fresh(repo, GetParam());
  fresh.Load();
  EXPECT_TRUE(fresh.Completed().count("CSCI100"));
}

TEST_P(ServiceTest, CompletingUnknownCourseThrows) {
  EXPECT_THROW(service.MarkCompleted("CSCI999"), CourseNotFoundError);
  EXPECT_EQ(repo.writes, 0);
}

INSTANTIATE_TEST_SUITE_P(BothTrees, ServiceTest,
                         ::testing::Values(TreeKind::AVL, TreeKind::BST),
                         [](const auto &info) {
                           return info.param == TreeKind::AVL ? "AVL" : "BST";
                         });

// ---- Validation on its own -------------------------------------------------

TEST(ServiceValidate, ReportsTheCycle) {
  std::vector<Course> looped = {{"A100", "A", {"C100"}},
                                {"B100", "B", {"A100"}},
                                {"C100", "C", {"B100"}}};
  try {
    CourseService::Validate(looped);
    FAIL() << "expected CyclicPrereqException";
  } catch (const CyclicPrereqException &e) {
    EXPECT_EQ(e.cycle().size(), 4u); // A -> B -> C -> A
  }
}

TEST(ServiceValidate, NormalizeId) {
  EXPECT_EQ(CourseService::NormalizeId("  csci100\t"), "CSCI100");
  EXPECT_EQ(CourseService::NormalizeId(""), "");
}

TEST(ServiceLoad, BadCatalogLeavesServiceUnloaded) {
  InMemoryRepository repo;
  // The fake accepts this (no cycle check in storage); the service must not.
  repo.ReplaceAll({{"A100", "A", {}}, {"B100", "B", {"A100"}}});
  repo.Update({"A100", "A", {"B100"}});

  CourseService service(repo);
  EXPECT_THROW(service.Load(), CyclicPrereqException);
  EXPECT_FALSE(service.Loaded());
  EXPECT_THROW(service.AddCourse({"C100", "C", {}}), std::logic_error);
}

// ---- Over the real backends ------------------------------------------------

TEST(ServiceOverCsv, LoadsAndIsReadOnly) {
  CsvRepository csv(TEST_DATA_DIR "/CS300_ABCU_Input.csv");
  CourseService service(csv);
  service.Load();
  EXPECT_EQ(ids(service.AllCoursesSorted()), kSampleIds);
  EXPECT_FALSE(service.CanEdit());
  EXPECT_THROW(service.AddCourse({"CSCI500", "Capstone", {}}),
               ReadOnlyRepositoryError);
  EXPECT_FALSE(service.Find("CSCI500").has_value());
}

TEST(ServiceOverSqlite, ImportCsvThenEditPersists) {
  SqliteRepository db(":memory:");
  CourseService service(db);
  service.Load();
  EXPECT_EQ(service.CourseCount(), 0u);

  CsvRepository csv(TEST_DATA_DIR "/CS300_ABCU_Input.csv");
  EXPECT_EQ(service.ImportFrom(csv), 8u);
  service.AddCourse({"CSCI500", "Capstone", {"CSCI400"}});
  service.MarkCompleted("CSCI100");

  // A second service over the same database sees everything.
  CourseService again(db);
  again.Load();
  EXPECT_EQ(again.CourseCount(), 9u);
  EXPECT_EQ(again.Find("CSCI500")->prereq, Ids{"CSCI400"});
  EXPECT_TRUE(again.Completed().count("CSCI100"));
}

TEST(ServiceOverSqlite, FailedImportLeavesDatabaseUnchanged) {
  SqliteRepository db(":memory:");
  db.ReplaceAll(kSample);
  CourseService service(db);
  service.Load();

  InMemoryRepository looped;
  looped.ReplaceAll({{"A100", "A", {}}, {"B100", "B", {"A100"}}});
  looped.Update({"A100", "A", {"B100"}});

  EXPECT_THROW(service.ImportFrom(looped), CyclicPrereqException);
  EXPECT_EQ(ids(db.LoadAll()), kSampleIds);
  EXPECT_EQ(ids(service.AllCoursesSorted()), kSampleIds);
}

TEST(ServiceOverSqlite, InMemoryEligibilityMatchesTheSqlQuery) {
  // The service works out eligibility from its cache; SqliteRepository has a
  // pure-SQL version. Walk through a whole degree and check they agree at
  // every step.
  SqliteRepository db(":memory:");
  db.ReplaceAll(kSample);
  CourseService service(db);
  service.Load();

  for (const auto &id : service.Graph().TopologicalOrder()) {
    EXPECT_EQ(ids(service.EligibleCourses()), ids(db.EligibleCourses()))
        << "before completing " << id;
    service.MarkCompleted(id);
  }
  EXPECT_TRUE(service.EligibleCourses().empty());
  EXPECT_TRUE(db.EligibleCourses().empty());
}

} // namespace
