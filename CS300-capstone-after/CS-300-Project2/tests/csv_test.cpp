#include "../csv.h"
#include <gtest/gtest.h>

TEST(FileToRows, ValidFileLoads) {
  std::vector<std::string> lines = {"CS100,Intro to CS",
                                    "CS200,Data Structures,CS100"};
  Rows rows;
  EXPECT_NO_THROW(rows = FileToRows(lines));
  EXPECT_EQ(rows.size(), 2u);
}

TEST(FileToRows, WindowsLineEndingsAreTrimmed) {
  std::vector<std::string> lines = {"CS100,Intro\r", "CS200,DS,CS100\r"};
  Rows rows = FileToRows(lines);
  EXPECT_EQ(rows[1][2], "CS100"); // no trailing \r
}

TEST(FileToRows, TrailingCommaIsIgnored) {
  std::vector<std::string> lines = {"CS100,Intro,,"};
  Rows rows = FileToRows(lines);
  EXPECT_EQ(rows[0].size(), 2u);
}

TEST(FileToRows, MissingIdThrows) {
  std::vector<std::string> lines = {"CS100,Intro", ",No ID"};
  EXPECT_THROW(FileToRows(lines), CSVMissingCourseIDException);
}

TEST(FileToRows, UnknownPrereqThrows) {
  std::vector<std::string> lines = {"CS200,DS,CS100"};
  EXPECT_THROW(FileToRows(lines), CSVPrerequisiteNotCourseException);
}

TEST(FileToRows, LineNumberCountsBlankLines) {
  std::vector<std::string> lines = {"CS100,Intro", "", ",No ID"};
  try {
    FileToRows(lines);
    FAIL() << "expected CSVMissingCourseIDException";
  } catch (const CSVMissingCourseIDException &e) {
    EXPECT_NE(std::string(e.what()).find("3"), std::string::npos);
  }
}
