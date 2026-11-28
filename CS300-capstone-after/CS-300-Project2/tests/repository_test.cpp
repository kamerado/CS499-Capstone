// Tests for the storage layer.
//
// 1. Contract tests: one suite, written only against ICourseRepository, run
//    against every writable implementation (SQLite in memory, SQLite on disk,
//    and the in-memory test fake). Any new backend just adds a factory.
// 2. SqliteRepository specifics: persistence, schema, injection safety.
// 3. CsvRepository: loading the ABCU format, read-only behaviour.

#include <gtest/gtest.h>

#include <sqlite3.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "course_repository.h"
#include "csv.h"
#include "csv_repository.h"
#include "fake_repository.h"
#include "sqlite_repository.h"

namespace fs = std::filesystem;
using Ids = std::vector<std::string>;

namespace {

// A temporary file path, deleted (with SQLite's side files) when it goes away.
class TempPath {
public:
  explicit TempPath(const std::string &ext) {
    // ctest may run several test processes at once, so each process gets
    // its own random prefix (std::random_device, not getpid(), which is
    // POSIX-only).
    static const std::string prefix = std::to_string(std::random_device{}());
    static std::atomic<int> counter{0};
    path_ = fs::temp_directory_path() /
            ("repo_test_" + prefix + "_" + std::to_string(counter++) + ext);
  }
  ~TempPath() {
    std::error_code ignored;
    for (const char *suffix : {"", "-journal", "-wal", "-shm"})
      fs::remove(path_.string() + suffix, ignored);
  }
  std::string str() const { return path_.string(); }
  void write(const std::string &contents) const {
    std::ofstream(path_, std::ios::binary) << contents;
  }

private:
  fs::path path_;
};

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

// ===========================================================================
// 1. Contract tests
// ===========================================================================

struct Backend {
  std::string name;
  std::function<std::unique_ptr<ICourseRepository>()> make;
};

void PrintTo(const Backend &b, std::ostream *os) { *os << b.name; }

std::shared_ptr<TempPath> diskDb = std::make_shared<TempPath>(".db");

std::vector<Backend> Backends() {
  return {
      {"SqliteMemory",
       [] { return std::make_unique<SqliteRepository>(":memory:"); }},
      {"SqliteFile",
       [] {
         // Fresh file per test.
         std::error_code ignored;
         fs::remove(diskDb->str(), ignored);
         return std::make_unique<SqliteRepository>(diskDb->str());
       }},
      {"InMemoryFake", [] { return std::make_unique<InMemoryRepository>(); }},
  };
}

class RepositoryContract : public ::testing::TestWithParam<Backend> {
protected:
  void SetUp() override { repo = GetParam().make(); }
  void loadSample() { repo->ReplaceAll(kSample); }
  std::unique_ptr<ICourseRepository> repo;
};

TEST_P(RepositoryContract, StartsEmptyAndWritable) {
  EXPECT_TRUE(repo->LoadAll().empty());
  EXPECT_TRUE(repo->LoadCompleted().empty());
  EXPECT_FALSE(repo->IsReadOnly());
  EXPECT_FALSE(repo->Find("CSCI100").has_value());
}

TEST_P(RepositoryContract, AddThenFind) {
  repo->Add({"CSCI100", "Intro", {}});
  repo->Add({"CSCI101", "Programming", {"CSCI100"}});

  auto c = repo->Find("CSCI101");
  ASSERT_TRUE(c.has_value());
  EXPECT_EQ(c->courseTitle, "Programming");
  EXPECT_EQ(c->prereq, Ids{"CSCI100"});
}

TEST_P(RepositoryContract, LoadAllIsSortedAndPrereqsAreSortedAndUnique) {
  repo->Add({"MATH201", "Discrete", {}});
  repo->Add({"CSCI200", "DS", {}});
  repo->Add({"CSCI300", "Algorithms", {"MATH201", "CSCI200", "MATH201"}});

  auto all = repo->LoadAll();
  EXPECT_EQ(ids(all), (Ids{"CSCI200", "CSCI300", "MATH201"}));
  EXPECT_EQ(all[1].prereq, (Ids{"CSCI200", "MATH201"}));
}

TEST_P(RepositoryContract, DuplicateAddIsRejectedAndOriginalKept) {
  repo->Add({"CSCI100", "Original", {}});
  EXPECT_THROW(repo->Add({"CSCI100", "Impostor", {}}), DuplicateCourseError);
  EXPECT_EQ(repo->Find("CSCI100")->courseTitle, "Original");
}

TEST_P(RepositoryContract, AddWithUnknownPrereqChangesNothing) {
  repo->Add({"CSCI100", "Intro", {}});
  EXPECT_THROW(repo->Add({"CSCI200", "DS", {"CSCI100", "NOPE999"}}),
               UnknownCourseError);
  // All-or-nothing: the course row must not be left behind either.
  EXPECT_FALSE(repo->Find("CSCI200").has_value());
  EXPECT_EQ(repo->LoadAll().size(), 1u);
}

TEST_P(RepositoryContract, SelfPrerequisiteIsRejected) {
  EXPECT_THROW(repo->Add({"CSCI100", "Intro", {"CSCI100"}}),
               SelfPrerequisiteError);
  EXPECT_TRUE(repo->LoadAll().empty());
}

TEST_P(RepositoryContract, UpdateReplacesTitleAndPrereqs) {
  loadSample();
  repo->Update({"CSCI300", "Algorithms II", {"CSCI301"}});
  auto c = repo->Find("CSCI300");
  EXPECT_EQ(c->courseTitle, "Algorithms II");
  EXPECT_EQ(c->prereq, Ids{"CSCI301"});
}

TEST_P(RepositoryContract, UpdateMissingCourseThrows) {
  EXPECT_THROW(repo->Update({"NOPE999", "x", {}}), CourseNotFoundError);
}

TEST_P(RepositoryContract, FailedUpdateKeepsOldPrereqs) {
  loadSample();
  EXPECT_THROW(repo->Update({"CSCI300", "New", {"CSCI200", "NOPE999"}}),
               UnknownCourseError);
  auto c = repo->Find("CSCI300");
  EXPECT_EQ(c->courseTitle, "Introduction to Algorithms");
  EXPECT_EQ(c->prereq, (Ids{"CSCI200", "MATH201"}));
}

TEST_P(RepositoryContract, RemoveDeletesCourseAndItsPrereqRows) {
  loadSample();
  repo->Remove("CSCI400"); // nothing depends on it
  EXPECT_FALSE(repo->Find("CSCI400").has_value());
  EXPECT_EQ(repo->LoadAll().size(), kSample.size() - 1);
}

TEST_P(RepositoryContract, RemoveMissingCourseThrows) {
  EXPECT_THROW(repo->Remove("NOPE999"), CourseNotFoundError);
}

TEST_P(RepositoryContract, RemoveCourseInUseNamesDependents) {
  loadSample();
  try {
    repo->Remove("CSCI101");
    FAIL() << "expected CourseInUseError";
  } catch (const CourseInUseError &e) {
    EXPECT_EQ(e.dependents(), (Ids{"CSCI200", "CSCI301"}));
  }
  EXPECT_TRUE(repo->Find("CSCI101").has_value());
}

TEST_P(RepositoryContract, ReplaceAllAcceptsPrereqsListedBeforeTheirCourse) {
  // CSCI101 appears before CSCI100 here, like in the ABCU CSV file.
  repo->ReplaceAll(
      {{"CSCI101", "Programming", {"CSCI100"}}, {"CSCI100", "Intro", {}}});
  EXPECT_EQ(ids(repo->LoadAll()), (Ids{"CSCI100", "CSCI101"}));
}

TEST_P(RepositoryContract, ReplaceAllReplacesCatalogAndClearsCompleted) {
  loadSample();
  repo->SetCompleted("CSCI100", true);
  repo->ReplaceAll({{"BIO100", "Biology", {}}});
  EXPECT_EQ(ids(repo->LoadAll()), Ids{"BIO100"});
  EXPECT_TRUE(repo->LoadCompleted().empty());
}

TEST_P(RepositoryContract, FailedReplaceAllLeavesOldCatalog) {
  loadSample();
  repo->SetCompleted("CSCI100", true);
  EXPECT_THROW(repo->ReplaceAll({{"A100", "A", {}}, {"B100", "B", {"ZZZ"}}}),
               UnknownCourseError);
  EXPECT_EQ(repo->LoadAll().size(), kSample.size());
  EXPECT_EQ(repo->LoadCompleted(), Ids{"CSCI100"});
}

TEST_P(RepositoryContract, ReplaceAllRejectsDuplicates) {
  EXPECT_THROW(repo->ReplaceAll({{"A100", "A", {}}, {"A100", "Again", {}}}),
               DuplicateCourseError);
  EXPECT_TRUE(repo->LoadAll().empty());
}

TEST_P(RepositoryContract, CompletedCoursesRoundTrip) {
  loadSample();
  repo->SetCompleted("MATH201", true);
  repo->SetCompleted("CSCI100", true);
  repo->SetCompleted("CSCI100", true); // idempotent
  EXPECT_EQ(repo->LoadCompleted(), (Ids{"CSCI100", "MATH201"}));

  repo->SetCompleted("MATH201", false);
  EXPECT_EQ(repo->LoadCompleted(), Ids{"CSCI100"});
}

TEST_P(RepositoryContract, CompletingUnknownCourseThrows) {
  EXPECT_THROW(repo->SetCompleted("NOPE999", true), CourseNotFoundError);
}

TEST_P(RepositoryContract, RemovingCourseClearsItsCompletedMark) {
  loadSample();
  repo->SetCompleted("CSCI400", true);
  repo->Remove("CSCI400");
  EXPECT_TRUE(repo->LoadCompleted().empty());
}

INSTANTIATE_TEST_SUITE_P(AllBackends, RepositoryContract,
                         ::testing::ValuesIn(Backends()),
                         [](const auto &info) { return info.param.name; });

// ===========================================================================
// 2. SqliteRepository specifics
// ===========================================================================

TEST(SqliteRepository, DataSurvivesReopening) {
  TempPath db(".db");
  {
    SqliteRepository repo(db.str());
    repo.ReplaceAll(kSample);
    repo.SetCompleted("CSCI100", true);
  }
  SqliteRepository reopened(db.str());
  EXPECT_EQ(reopened.LoadAll(), [] {
    auto sorted = kSample;
    std::sort(sorted.begin(), sorted.end(),
              [](auto &a, auto &b) { return a.courseID < b.courseID; });
    return sorted;
  }());
  EXPECT_EQ(reopened.LoadCompleted(), Ids{"CSCI100"});
}

TEST(SqliteRepository, InputIsBoundNotConcatenated) {
  // If IDs were pasted into SQL, this would end the statement and drop the
  // table. With bound parameters it's just an odd course ID.
  SqliteRepository repo(":memory:");
  const std::string evil = "X'); DROP TABLE courses; --";
  repo.Add({evil, "Robert'); DROP TABLE students;--", {}});

  auto c = repo.Find(evil);
  ASSERT_TRUE(c.has_value());
  EXPECT_EQ(c->courseTitle, "Robert'); DROP TABLE students;--");
  EXPECT_FALSE(repo.Find("X").has_value());
  repo.Add({"CSCI100", "Intro", {}}); // courses table still exists
  EXPECT_EQ(repo.LoadAll().size(), 2u);
}

TEST(SqliteRepository, SchemaEnforcesIntegrityOnItsOwn) {
  // Write straight to the file, bypassing all C++ checks: the database
  // constraints must still reject bad data.
  TempPath db(".db");
  {
    SqliteRepository create(db.str());
  }

  sqlite3 *raw = nullptr;
  ASSERT_EQ(sqlite3_open(db.str().c_str(), &raw), SQLITE_OK);
  sqlite3_exec(raw, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr);
  auto run = [&](const char *sql) {
    return sqlite3_exec(raw, sql, nullptr, nullptr, nullptr);
  };
  EXPECT_EQ(run("INSERT INTO courses VALUES ('A100', 'A')"), SQLITE_OK);
  EXPECT_NE(run("INSERT INTO courses VALUES ('A100', 'dup')"), SQLITE_OK);
  EXPECT_NE(run("INSERT INTO courses VALUES ('B100', NULL)"), SQLITE_OK);
  EXPECT_NE(run("INSERT INTO prerequisites VALUES ('A100', 'A100')"),
            SQLITE_OK);
  EXPECT_NE(run("INSERT INTO prerequisites VALUES ('A100', 'GHOST')"),
            SQLITE_OK);
  EXPECT_NE(run("INSERT INTO completed_courses VALUES ('GHOST')"), SQLITE_OK);
  sqlite3_close(raw);
}

TEST(SqliteRepository, RecordsSchemaVersion) {
  TempPath db(".db");
  {
    SqliteRepository create(db.str());
  }
  sqlite3 *raw = nullptr;
  sqlite3_open(db.str().c_str(), &raw);
  sqlite3_stmt *stmt = nullptr;
  sqlite3_prepare_v2(raw, "PRAGMA user_version", -1, &stmt, nullptr);
  ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
  EXPECT_EQ(sqlite3_column_int(stmt, 0), SqliteRepository::kSchemaVersion);
  sqlite3_finalize(stmt);
  sqlite3_close(raw);
}

TEST(SqliteRepository, RefusesNewerSchema) {
  TempPath db(".db");
  {
    sqlite3 *raw = nullptr;
    sqlite3_open(db.str().c_str(), &raw);
    sqlite3_exec(raw, "PRAGMA user_version = 99", nullptr, nullptr, nullptr);
    sqlite3_close(raw);
  }
  EXPECT_THROW(SqliteRepository repo(db.str()), RepositoryError);
}

TEST(SqliteRepository, NonDatabaseFileIsAnError) {
  TempPath notDb(".db");
  notDb.write("CSCI100,Intro\nthis is a csv, not a database\n");
  EXPECT_THROW(SqliteRepository repo(notDb.str()), RepositoryError);
}

TEST(SqliteRepository, EligibleCoursesQuery) {
  SqliteRepository repo(":memory:");
  repo.ReplaceAll(kSample);
  EXPECT_EQ(ids(repo.EligibleCourses()), (Ids{"CSCI100", "MATH201"}));

  repo.SetCompleted("CSCI100", true);
  EXPECT_EQ(ids(repo.EligibleCourses()), (Ids{"CSCI101", "MATH201"}));

  repo.SetCompleted("CSCI101", true);
  repo.SetCompleted("CSCI200", true);
  // CSCI300 still needs MATH201.
  EXPECT_EQ(ids(repo.EligibleCourses()), (Ids{"CSCI301", "MATH201"}));
}

TEST(SqliteRepository, IsMovable) {
  SqliteRepository a(":memory:");
  a.Add({"CSCI100", "Intro", {}});
  SqliteRepository b = std::move(a);
  EXPECT_TRUE(b.Find("CSCI100").has_value());
}

// ===========================================================================
// 3. CsvRepository
// ===========================================================================

TEST(CsvRepository, LoadsTheAbcuSampleFile) {
  CsvRepository repo(TEST_DATA_DIR "/CS300_ABCU_Input.csv");
  auto all = repo.LoadAll();
  EXPECT_EQ(ids(all), (Ids{"CSCI100", "CSCI101", "CSCI200", "CSCI300",
                           "CSCI301", "CSCI350", "CSCI400", "MATH201"}));
  EXPECT_EQ(repo.Find("CSCI300")->prereq, (Ids{"CSCI200", "MATH201"}));
  EXPECT_FALSE(repo.Find("CSCI999").has_value());
}

TEST(CsvRepository, IsReadOnly) {
  CsvRepository repo(TEST_DATA_DIR "/CS300_ABCU_Input.csv");
  EXPECT_TRUE(repo.IsReadOnly());
  EXPECT_THROW(repo.Add({"A", "B", {}}), ReadOnlyRepositoryError);
  EXPECT_THROW(repo.Update({"CSCI100", "B", {}}), ReadOnlyRepositoryError);
  EXPECT_THROW(repo.Remove("CSCI100"), ReadOnlyRepositoryError);
  EXPECT_THROW(repo.ReplaceAll({}), ReadOnlyRepositoryError);
  EXPECT_THROW(repo.SetCompleted("CSCI100", true), ReadOnlyRepositoryError);
  EXPECT_TRUE(repo.LoadCompleted().empty());
}

TEST(CsvRepository, MissingFileIsAnError) {
  CsvRepository repo("definitely_not_here.csv");
  EXPECT_THROW(repo.LoadAll(), RepositoryError);
}

TEST(CsvRepository, BadRowKeepsTheCsvLineNumber) {
  TempPath csv(".csv");
  csv.write("CSCI100,Intro\n,No ID\n");
  CsvRepository repo(csv.str());
  try {
    repo.LoadAll();
    FAIL() << "expected a CSV error";
  } catch (const CSVMissingCourseIDException &e) {
    EXPECT_NE(std::string(e.what()).find("line: 2"), std::string::npos);
  }
}

TEST(CsvRepository, RepeatedPrereqIsKeptOnce) {
  TempPath csv(".csv");
  csv.write("CSCI100,Intro\nCSCI200,DS,CSCI100,CSCI100\n");
  CsvRepository repo(csv.str());
  EXPECT_EQ(repo.Find("CSCI200")->prereq, Ids{"CSCI100"});
}

} // namespace
