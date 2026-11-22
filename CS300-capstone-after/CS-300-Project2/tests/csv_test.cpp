// Unit tests for the CSV layer (csv.h / csv.cpp): trim, readFile, verifyFile,
// FileToRows, and the exception types.
//
// Most tests feed FileToRows a vector of lines directly, so they don't touch
// the disk. The readFile tests write small temporary files, which TempFile
// deletes again when the test ends.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "csv.h"

namespace {

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

using Lines = std::vector<std::string>;

// A temporary file with the given contents, removed when it goes out of scope.
// Written in binary mode so "\r\n" stays exactly as given.
class TempFile {
public:
  explicit TempFile(const std::string &contents) {
    const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
    path_ = std::filesystem::temp_directory_path() /
            (std::string("csv_test_") + info->test_suite_name() + "_" +
             info->name() + "_" + std::to_string(counter_++) + ".csv");
    std::ofstream out(path_, std::ios::binary);
    out << contents;
  }
  ~TempFile() {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
  }
  TempFile(const TempFile &) = delete;
  TempFile &operator=(const TempFile &) = delete;

  std::string path() const { return path_.string(); }

private:
  std::filesystem::path path_;
  static inline int counter_ = 0;
};

// Runs FileToRows and checks that it throws exactly Ex with exactly this
// message. Checking the message also checks the line number or course ID.
template <typename Ex>
void ExpectThrowWithMessage(Lines lines, const std::string &message) {
  try {
    FileToRows(lines);
    FAIL() << "expected an exception with message: " << message;
  } catch (const Ex &e) {
    EXPECT_EQ(std::string(e.what()), message);
  }
}

} // namespace

// ---------------------------------------------------------------------------
// trim
// ---------------------------------------------------------------------------

TEST(Trim, RemovesSpacesFromBothEnds) { EXPECT_EQ(trim("  CS100  "), "CS100"); }

TEST(Trim, RemovesTabsAndLineEndings) {
  EXPECT_EQ(trim("\tCS100\r\n"), "CS100");
}

TEST(Trim, KeepsInteriorWhitespace) {
  EXPECT_EQ(trim("  Intro to CS  "), "Intro to CS");
}

TEST(Trim, AllWhitespaceBecomesEmpty) { EXPECT_EQ(trim(" \t\r\n "), ""); }

TEST(Trim, EmptyStaysEmpty) { EXPECT_EQ(trim(""), ""); }

TEST(Trim, NothingToTrimIsUnchanged) { EXPECT_EQ(trim("CS100"), "CS100"); }

TEST(Trim, SingleCharacterSurvives) { EXPECT_EQ(trim(" x "), "x"); }

// ---------------------------------------------------------------------------
// FileToRows: valid input
// ---------------------------------------------------------------------------

TEST(FileToRows, ValidFileLoads) {
  Lines lines = {"CS100,Intro to CS", "CS200,Data Structures,CS100"};
  Rows rows;
  EXPECT_NO_THROW(rows = FileToRows(lines));
  EXPECT_EQ(rows.size(), 2u);
}

TEST(FileToRows, ParsesEveryField) {
  Lines lines = {"CS100,Intro to CS", "CS200,Data Structures,CS100",
                 "CS300,Algorithms,CS100,CS200"};
  Rows rows = FileToRows(lines);

  ASSERT_EQ(rows.size(), 3u);
  EXPECT_EQ(rows[0], (Row{"CS100", "Intro to CS"}));
  EXPECT_EQ(rows[1], (Row{"CS200", "Data Structures", "CS100"}));
  EXPECT_EQ(rows[2], (Row{"CS300", "Algorithms", "CS100", "CS200"}));
}

TEST(FileToRows, EmptyInputGivesNoRows) {
  Lines lines;
  Rows rows;
  EXPECT_NO_THROW(rows = FileToRows(lines));
  EXPECT_TRUE(rows.empty());
}

TEST(FileToRows, OnlyBlankLinesGivesNoRows) {
  Lines lines = {"", "   ", "\r", "\t"};
  EXPECT_TRUE(FileToRows(lines).empty());
}

TEST(FileToRows, KeepsFileOrder) {
  Lines lines = {"MATH201,Discrete Math", "CS100,Intro", "CS050,Prep"};
  Rows rows = FileToRows(lines);
  ASSERT_EQ(rows.size(), 3u);
  EXPECT_EQ(rows[0][0], "MATH201");
  EXPECT_EQ(rows[1][0], "CS100");
  EXPECT_EQ(rows[2][0], "CS050");
}

TEST(FileToRows, DoesNotModifyInput) {
  Lines lines = {" CS100 , Intro \r", "", "CS200,DS,CS100,"};
  Lines original = lines;
  FileToRows(lines);
  EXPECT_EQ(lines, original);
}

// ---------------------------------------------------------------------------
// FileToRows: cleanup of messy input
// ---------------------------------------------------------------------------

TEST(FileToRows, WindowsLineEndingsAreTrimmed) {
  Lines lines = {"CS100,Intro\r", "CS200,DS,CS100\r"};
  Rows rows = FileToRows(lines);
  EXPECT_EQ(rows[0][1], "Intro");
  EXPECT_EQ(rows[1][2], "CS100"); // no trailing \r
}

TEST(FileToRows, WhitespaceAroundFieldsIsTrimmed) {
  Lines lines = {"  CS100  ,\tIntro to CS\t", " CS200 , DS , CS100 "};
  Rows rows = FileToRows(lines);
  EXPECT_EQ(rows[0], (Row{"CS100", "Intro to CS"}));
  EXPECT_EQ(rows[1], (Row{"CS200", "DS", "CS100"}));
}

TEST(FileToRows, TrailingCommaIsIgnored) {
  Lines lines = {"CS100,Intro,,"};
  Rows rows = FileToRows(lines);
  EXPECT_EQ(rows[0].size(), 2u);
}

TEST(FileToRows, DoubledCommaBetweenPrereqsIsIgnored) {
  Lines lines = {"CS100,Intro", "CS150,Prep", "CS200,DS,CS100,,CS150"};
  Rows rows = FileToRows(lines);
  EXPECT_EQ(rows[2], (Row{"CS200", "DS", "CS100", "CS150"}));
}

TEST(FileToRows, WhitespaceOnlyPrereqIsIgnored) {
  Lines lines = {"CS100,Intro,   ,\t"};
  Rows rows = FileToRows(lines);
  EXPECT_EQ(rows[0], (Row{"CS100", "Intro"}));
}

TEST(FileToRows, BlankLinesAreSkipped) {
  Lines lines = {"", "CS100,Intro", "   ", "CS200,DS,CS100", "\r"};
  EXPECT_EQ(FileToRows(lines).size(), 2u);
}

// Prerequisites may refer to courses that appear later in the file.
TEST(FileToRows, PrereqCanBeDefinedLater) {
  Lines lines = {"CS200,DS,CS100", "CS100,Intro"};
  EXPECT_NO_THROW(FileToRows(lines));
}

// ---------------------------------------------------------------------------
// FileToRows: invalid rows (each checks the exact message and line number)
// ---------------------------------------------------------------------------

TEST(FileToRows, SingleColumnThrowsInvalidFormat) {
  ExpectThrowWithMessage<CSVInvalidFormatException>(
      {"CS100,Intro", "CS200"},
      "CSV line: 2 invalid format: less than two columns");
}

TEST(FileToRows, MissingIdThrows) {
  ExpectThrowWithMessage<CSVMissingCourseIDException>(
      {"CS100,Intro", ",No ID"}, "CSV line: 2 is missing ID field.");
}

TEST(FileToRows, WhitespaceOnlyIdThrowsMissingId) {
  ExpectThrowWithMessage<CSVMissingCourseIDException>(
      {"   ,Title"}, "CSV line: 1 is missing ID field.");
}

TEST(FileToRows, LoneCommaThrowsMissingId) {
  ExpectThrowWithMessage<CSVMissingCourseIDException>(
      {","}, "CSV line: 1 is missing ID field.");
}

TEST(FileToRows, MissingTitleThrows) {
  ExpectThrowWithMessage<CSVMissingCourseTitleException>(
      {"CS100,"}, "CSV line: 1 is missing Title field.");
}

// The empty title must not be hidden by the empty prerequisite cleanup.
TEST(FileToRows, MissingTitleWithTrailingCommasThrows) {
  ExpectThrowWithMessage<CSVMissingCourseTitleException>(
      {"CS100,,,"}, "CSV line: 1 is missing Title field.");
}

TEST(FileToRows, SelfPrerequisiteThrows) {
  ExpectThrowWithMessage<CSVSelfIsPrereqException>(
      {"CS100,Intro", "CS200,DS,CS100,CS200"},
      "CSV line: 2 course lists itself as a prerequisite.");
}

TEST(FileToRows, DuplicateCourseThrows) {
  ExpectThrowWithMessage<CSVDuplicateCourseException>(
      {"CS100,Intro", "CS200,DS", "CS100,Intro Again"},
      "CS100 is duplicate course.");
}

TEST(FileToRows, DuplicateAfterTrimmingThrows) {
  ExpectThrowWithMessage<CSVDuplicateCourseException>(
      {"CS100,Intro", " CS100\r,Intro"}, "CS100 is duplicate course.");
}

TEST(FileToRows, UnknownPrereqThrows) {
  ExpectThrowWithMessage<CSVPrerequisiteNotCourseException>(
      {"CS200,DS,CS100"}, "CS200 lists unknown prerequisite CS100.");
}

// Course IDs are compared exactly, so a lowercase prereq is unknown.
TEST(FileToRows, PrereqMatchIsCaseSensitive) {
  ExpectThrowWithMessage<CSVPrerequisiteNotCourseException>(
      {"CS100,Intro", "CS200,DS,cs100"},
      "CS200 lists unknown prerequisite cs100.");
}

// Line numbers count every input line, including blank ones, so they match
// what the user sees in an editor.
TEST(FileToRows, LineNumberCountsBlankLines) {
  ExpectThrowWithMessage<CSVMissingCourseIDException>(
      {"CS100,Intro", "", ",No ID"}, "CSV line: 3 is missing ID field.");
}

TEST(FileToRows, FirstErrorInFileIsReported) {
  ExpectThrowWithMessage<CSVMissingCourseTitleException>(
      {"CS100,Intro", "CS200,", ",No ID"},
      "CSV line: 2 is missing Title field.");
}

// ---------------------------------------------------------------------------
// Exception hierarchy: callers can catch broadly
// ---------------------------------------------------------------------------

TEST(CSVExceptions, AllRowErrorsAreCSVExceptions) {
  std::vector<Lines> badFiles = {
      {"CS100"},                    // invalid format
      {",Intro"},                   // missing ID
      {"CS100,"},                   // missing title
      {"CS100,Intro,CS100"},        // self prerequisite
      {"CS100,Intro", "CS100,Dup"}, // duplicate
      {"CS200,DS,CS100"},           // unknown prerequisite
  };
  for (auto &lines : badFiles) {
    EXPECT_THROW(FileToRows(lines), CSVException) << lines[0];
  }
}

TEST(CSVExceptions, CSVExceptionIsARuntimeError) {
  Lines lines = {"CS100"};
  EXPECT_THROW(FileToRows(lines), std::runtime_error);
}

TEST(CSVExceptions, EmptyFileExceptionHasMessage) {
  EmptyFileException e;
  EXPECT_STREQ(e.what(), "File is empty.");
}

// ---------------------------------------------------------------------------
// verifyFile (called directly with rows that are already parsed)
// ---------------------------------------------------------------------------

TEST(VerifyFile, ValidRowsReturnTrue) {
  Rows rows = {{"CS100", "Intro"}, {"CS200", "DS", "CS100"}};
  EXPECT_TRUE(verifyFile(rows));
}

TEST(VerifyFile, EmptyRowsAreValid) {
  Rows rows;
  EXPECT_TRUE(verifyFile(rows));
}

TEST(VerifyFile, DuplicateIdThrows) {
  Rows rows = {{"CS100", "Intro"}, {"CS100", "Again"}};
  EXPECT_THROW(verifyFile(rows), CSVDuplicateCourseException);
}

TEST(VerifyFile, UnknownPrereqThrows) {
  Rows rows = {{"CS200", "DS", "CS999"}};
  EXPECT_THROW(verifyFile(rows), CSVPrerequisiteNotCourseException);
}

TEST(VerifyFile, ForwardReferenceIsAllowed) {
  Rows rows = {{"CS300", "Algo", "CS200"}, {"CS200", "DS"}};
  EXPECT_TRUE(verifyFile(rows));
}

// ---------------------------------------------------------------------------
// readFile (uses real temporary files)
// ---------------------------------------------------------------------------

TEST(ReadFile, ReadsEveryLine) {
  TempFile file("CS100,Intro\nCS200,DS,CS100\n");
  Lines lines;
  readFile(file.path(), lines);
  EXPECT_EQ(lines, (Lines{"CS100,Intro", "CS200,DS,CS100"}));
}

TEST(ReadFile, ReadsLastLineWithoutNewline) {
  TempFile file("CS100,Intro\nCS200,DS");
  Lines lines;
  readFile(file.path(), lines);
  EXPECT_EQ(lines, (Lines{"CS100,Intro", "CS200,DS"}));
}

TEST(ReadFile, SkipsEmptyLines) {
  TempFile file("CS100,Intro\n\n\nCS200,DS\n");
  Lines lines;
  readFile(file.path(), lines);
  EXPECT_EQ(lines.size(), 2u);
}

// ---------------------------------------------------------------------------
// readFile + FileToRows together, the way the app loads a catalog
// ---------------------------------------------------------------------------

TEST(LoadCatalog, WindowsFileLoadsCleanly) {
  // CRLF endings, including a blank CRLF line in the middle.
  TempFile file("CS100,Intro to CS\r\n\r\nCS200,Data Structures,CS100\r\n");
  Lines lines;
  readFile(file.path(), lines);

  Rows rows = FileToRows(lines);
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0], (Row{"CS100", "Intro to CS"}));
  EXPECT_EQ(rows[1], (Row{"CS200", "Data Structures", "CS100"}));
}

TEST(LoadCatalog, SampleCatalogLoads) {
  TempFile file("CSCI100,Introduction to Computer Science\n"
                "CSCI101,Introduction to Programming in C++,CSCI100\n"
                "CSCI200,Data Structures,CSCI101\n"
                "MATH201,Discrete Mathematics\n"
                "CSCI300,Introduction to Algorithms,CSCI200,MATH201\n"
                "CSCI301,Advanced Programming in C++,CSCI101\n"
                "CSCI350,Operating Systems,CSCI300\n"
                "CSCI400,Large Software Development,CSCI301,CSCI350\n");
  Lines lines;
  readFile(file.path(), lines);

  Rows rows = FileToRows(lines);
  ASSERT_EQ(rows.size(), 8u);
  EXPECT_EQ(rows[4], (Row{"CSCI300", "Introduction to Algorithms", "CSCI200",
                          "MATH201"}));
  EXPECT_EQ(rows[7].size(), 4u);
}

TEST(LoadCatalog, InvalidFileLineNumberMatchesFile) {
  // readFile drops truly empty lines, so the number is counted after that.
  TempFile file("CS100,Intro\nCS200,DS,CS100\nCS300\n");
  Lines lines;
  readFile(file.path(), lines);

  Rows rows;
  try {
    rows = FileToRows(lines);
    FAIL() << "expected CSVInvalidFormatException";
  } catch (const CSVInvalidFormatException &e) {
    EXPECT_EQ(std::string(e.what()),
              "CSV line: 3 invalid format: less than two columns");
  }
}

TEST(ReadFile, EmptyFileThrows) {
  TempFile file("");
  Lines lines;
  EXPECT_THROW(readFile(file.path(), lines), EmptyFileException);
}
