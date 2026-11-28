#include "sqlite_repository.h"

#include <sqlite3.h>

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace {

// A failed SQLite call, carrying the extended result code so callers can tell
// a primary-key clash from a foreign-key failure.
class SqliteError : public RepositoryError {
public:
  SqliteError(sqlite3 *db, const std::string &context)
      : RepositoryError(context + ": " + sqlite3_errmsg(db)),
        code_(sqlite3_extended_errcode(db)) {}
  int code() const { return code_; }

private:
  int code_;
};

// RAII wrapper for a prepared statement. Values are only ever passed in with
// bind(), never pasted into the SQL text.
class Statement {
public:
  Statement(sqlite3 *db, const char *sql) : db_(db) {
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt_, nullptr) != SQLITE_OK)
      throw SqliteError(db_, "Could not prepare query");
  }
  ~Statement() { sqlite3_finalize(stmt_); }
  Statement(const Statement &) = delete;
  Statement &operator=(const Statement &) = delete;

  Statement &bind(int index, const std::string &value) {
    // SQLITE_TRANSIENT: SQLite copies the string, so `value` may go away.
    if (sqlite3_bind_text(stmt_, index, value.c_str(),
                          static_cast<int>(value.size()),
                          SQLITE_TRANSIENT) != SQLITE_OK)
      throw SqliteError(db_, "Could not bind parameter");
    return *this;
  }

  // true while there is a row to read, false once the statement is done.
  bool step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW)
      return true;
    if (rc == SQLITE_DONE)
      return false;
    throw SqliteError(db_, "Query failed");
  }

  void run() {
    while (step()) {
    }
  }

  // Re-run the same statement with new parameters.
  void reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
  }

  std::string text(int column) const {
    const unsigned char *p = sqlite3_column_text(stmt_, column);
    return p ? reinterpret_cast<const char *>(p) : std::string();
  }
  int integer(int column) const { return sqlite3_column_int(stmt_, column); }

private:
  sqlite3 *db_;
  sqlite3_stmt *stmt_ = nullptr;
};

// RAII transaction: rolls back unless Commit() is reached, so an exception
// anywhere inside a write leaves the database unchanged.
class Transaction {
public:
  explicit Transaction(sqlite3 *db) : db_(db) {
    if (sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) !=
        SQLITE_OK)
      throw SqliteError(db_, "Could not start transaction");
  }
  ~Transaction() {
    if (!done_)
      sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
  }
  Transaction(const Transaction &) = delete;
  Transaction &operator=(const Transaction &) = delete;

  void Commit() {
    if (sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK)
      throw SqliteError(db_, "Could not commit");
    done_ = true;
  }

private:
  sqlite3 *db_;
  bool done_ = false;
};

// Sorted, without repeats, as the repository contract requires.
std::vector<std::string> normalizedPrereqs(const Course &course) {
  std::vector<std::string> p = course.prereq;
  std::sort(p.begin(), p.end());
  p.erase(std::unique(p.begin(), p.end()), p.end());
  return p;
}

constexpr const char *kSchema = R"sql(
CREATE TABLE IF NOT EXISTS courses (
    course_id TEXT PRIMARY KEY NOT NULL,
    title     TEXT NOT NULL CHECK (length(title) > 0)
);

CREATE TABLE IF NOT EXISTS prerequisites (
    course_id TEXT NOT NULL REFERENCES courses(course_id) ON DELETE CASCADE,
    prereq_id TEXT NOT NULL REFERENCES courses(course_id),
    PRIMARY KEY (course_id, prereq_id),
    CHECK (course_id <> prereq_id)
);

-- Finds "which courses need X?" without scanning the table.
CREATE INDEX IF NOT EXISTS idx_prerequisites_prereq
    ON prerequisites(prereq_id);

CREATE TABLE IF NOT EXISTS completed_courses (
    course_id TEXT PRIMARY KEY NOT NULL
              REFERENCES courses(course_id) ON DELETE CASCADE
);
)sql";

} // namespace

// ---- Connection ------------------------------------------------------------

void SqliteRepository::Closer::operator()(sqlite3 *db) const {
  sqlite3_close(db);
}

SqliteRepository::SqliteRepository(const std::string &path) : path_(path) {
  sqlite3 *raw = nullptr;
  int rc = sqlite3_open_v2(path.c_str(), &raw,
                           SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
  db_.reset(raw); // closed by Closer even if opening failed
  if (rc != SQLITE_OK)
    throw RepositoryError("Cannot open database " + path + ": " +
                          (raw ? sqlite3_errmsg(raw) : "out of memory"));

  sqlite3_extended_result_codes(db_.get(), 1);
  sqlite3_busy_timeout(db_.get(), 2000);

  // Foreign keys are off by default in SQLite and must be enabled on every
  // connection. Check it really took effect rather than trusting it.
  exec("PRAGMA foreign_keys = ON");
  Statement fk(db_.get(), "PRAGMA foreign_keys");
  if (!fk.step() || fk.integer(0) != 1)
    throw RepositoryError("This SQLite build does not support foreign keys.");

  createSchema();
}

SqliteRepository::~SqliteRepository() = default;
SqliteRepository::SqliteRepository(SqliteRepository &&) noexcept = default;
SqliteRepository &
SqliteRepository::operator=(SqliteRepository &&) noexcept = default;

void SqliteRepository::exec(const char *sql) {
  char *err = nullptr;
  if (sqlite3_exec(db_.get(), sql, nullptr, nullptr, &err) != SQLITE_OK) {
    std::string msg = err ? err : "unknown error";
    sqlite3_free(err);
    throw RepositoryError("Database error in " + path_ + ": " + msg);
  }
}

void SqliteRepository::createSchema() {
  Statement version(db_.get(), "PRAGMA user_version");
  version.step(); // fails here with "file is not a database" for a CSV etc.
  int current = version.integer(0);
  if (current > kSchemaVersion)
    throw RepositoryError(path_ + " was created by a newer version (schema " +
                          std::to_string(current) + ").");

  Transaction tx(db_.get());
  exec(kSchema);
  exec(("PRAGMA user_version = " + std::to_string(kSchemaVersion)).c_str());
  tx.Commit();
}

// ---- Reads -----------------------------------------------------------------

std::vector<Course> SqliteRepository::query(const char *sql) {
  // Prerequisites for every course in one pass, already sorted.
  std::unordered_map<std::string, std::vector<std::string>> prereqs;
  Statement p(db_.get(), "SELECT course_id, prereq_id FROM prerequisites "
                         "ORDER BY course_id, prereq_id");
  while (p.step())
    prereqs[p.text(0)].push_back(p.text(1));

  std::vector<Course> courses;
  Statement c(db_.get(), sql);
  while (c.step()) {
    std::string id = c.text(0);
    auto it = prereqs.find(id);
    courses.emplace_back(id, c.text(1),
                         it == prereqs.end() ? std::vector<std::string>{}
                                             : std::move(it->second));
  }
  return courses;
}

std::vector<Course> SqliteRepository::LoadAll() {
  return query("SELECT course_id, title FROM courses ORDER BY course_id");
}

std::optional<Course> SqliteRepository::Find(const std::string &id) {
  Statement c(db_.get(), "SELECT title FROM courses WHERE course_id = ?");
  c.bind(1, id);
  if (!c.step())
    return std::nullopt;
  Course course(id, c.text(0));

  Statement p(db_.get(), "SELECT prereq_id FROM prerequisites "
                         "WHERE course_id = ? ORDER BY prereq_id");
  p.bind(1, id);
  while (p.step())
    course.prereq.push_back(p.text(0));
  return course;
}

bool SqliteRepository::exists(const std::string &id) {
  Statement s(db_.get(), "SELECT 1 FROM courses WHERE course_id = ?");
  s.bind(1, id);
  return s.step();
}

std::vector<std::string> SqliteRepository::LoadCompleted() {
  std::vector<std::string> ids;
  Statement s(db_.get(),
              "SELECT course_id FROM completed_courses ORDER BY course_id");
  while (s.step())
    ids.push_back(s.text(0));
  return ids;
}

std::vector<Course> SqliteRepository::EligibleCourses() {
  return query(R"sql(
    SELECT c.course_id, c.title
    FROM courses c
    WHERE c.course_id NOT IN (SELECT course_id FROM completed_courses)
      AND NOT EXISTS (
          SELECT 1 FROM prerequisites p
          WHERE p.course_id = c.course_id
            AND p.prereq_id NOT IN (SELECT course_id FROM completed_courses))
    ORDER BY c.course_id
  )sql");
}

// ---- Writes ----------------------------------------------------------------
// Each public write opens a Transaction; the helpers below assume one is open.

void SqliteRepository::insertCourse(const Course &course) {
  Statement s(db_.get(),
              "INSERT INTO courses (course_id, title) VALUES (?, ?)");
  s.bind(1, course.courseID).bind(2, course.courseTitle);
  try {
    s.run();
  } catch (const SqliteError &e) {
    if (e.code() == SQLITE_CONSTRAINT_PRIMARYKEY)
      throw DuplicateCourseError(course.courseID);
    throw;
  }
}

void SqliteRepository::insertPrereqs(const Course &course) {
  Statement s(db_.get(), "INSERT INTO prerequisites (course_id, prereq_id) "
                         "VALUES (?, ?)");
  for (const auto &prereq : normalizedPrereqs(course)) {
    if (prereq == course.courseID)
      throw SelfPrerequisiteError(course.courseID);
    s.reset();
    s.bind(1, course.courseID).bind(2, prereq);
    try {
      s.run();
    } catch (const SqliteError &e) {
      if (e.code() == SQLITE_CONSTRAINT_FOREIGNKEY)
        throw UnknownCourseError(course.courseID, prereq);
      throw;
    }
  }
}

void SqliteRepository::Add(const Course &course) {
  Transaction tx(db_.get());
  insertCourse(course);
  insertPrereqs(course);
  tx.Commit();
}

void SqliteRepository::Update(const Course &course) {
  Transaction tx(db_.get());

  Statement title(db_.get(),
                  "UPDATE courses SET title = ? WHERE course_id = ?");
  title.bind(1, course.courseTitle).bind(2, course.courseID);
  title.run();
  if (sqlite3_changes(db_.get()) == 0)
    throw CourseNotFoundError(course.courseID);

  Statement clear(db_.get(), "DELETE FROM prerequisites WHERE course_id = ?");
  clear.bind(1, course.courseID);
  clear.run();

  insertPrereqs(course);
  tx.Commit();
}

void SqliteRepository::Remove(const std::string &id) {
  Transaction tx(db_.get());
  if (!exists(id))
    throw CourseNotFoundError(id);

  // The foreign key would block this delete anyway; asking first lets the
  // error say which courses still depend on it.
  Statement deps(db_.get(), "SELECT course_id FROM prerequisites "
                            "WHERE prereq_id = ? ORDER BY course_id");
  deps.bind(1, id);
  std::vector<std::string> dependents;
  while (deps.step())
    dependents.push_back(deps.text(0));
  if (!dependents.empty())
    throw CourseInUseError(id, dependents);

  // Its own prerequisite rows and completed mark go with it (ON DELETE
  // CASCADE).
  Statement del(db_.get(), "DELETE FROM courses WHERE course_id = ?");
  del.bind(1, id);
  del.run();
  tx.Commit();
}

void SqliteRepository::ReplaceAll(const std::vector<Course> &courses) {
  Transaction tx(db_.get());
  exec("DELETE FROM completed_courses");
  exec("DELETE FROM prerequisites");
  exec("DELETE FROM courses");

  // All courses first, then prerequisites, so a prerequisite listed before
  // its own row in the input still satisfies the foreign key.
  for (const auto &course : courses)
    insertCourse(course);
  for (const auto &course : courses)
    insertPrereqs(course);
  tx.Commit();
}

void SqliteRepository::SetCompleted(const std::string &id, bool completed) {
  Transaction tx(db_.get());
  if (!exists(id))
    throw CourseNotFoundError(id);

  Statement s(db_.get(),
              completed ? "INSERT OR IGNORE INTO completed_courses (course_id) "
                          "VALUES (?)"
                        : "DELETE FROM completed_courses WHERE course_id = ?");
  s.bind(1, id);
  s.run();
  tx.Commit();
}
