#include "bst.h"
#include "course.h"
#include <gtest/gtest.h>
#include <type_traits>

using Tree = BinarySearchTree<Course>;

static std::vector<std::string> keys(const Tree &t) {
  std::vector<std::string> out;
  t.InOrder([&](const Course &c) { out.push_back(c.getKey()); });
  return out;
}

static void insertAll(Tree &t, std::initializer_list<const char *> ids) {
  for (const char *id : ids)
    t.Insert(Course(id, "Title"));
}

// Compile-time proof of the memory fix: copies are impossible, moves work.
static_assert(!std::is_copy_constructible_v<Tree>);
static_assert(std::is_move_constructible_v<Tree>);

TEST(BST, EmptyTreeSearchReturnsNothing) {
  Tree t;
  EXPECT_FALSE(t.Search("CS100").has_value());
}

TEST(BST, InOrderIsSorted) {
  Tree t;
  insertAll(t, {"D", "B", "F", "A", "C", "E", "G"});
  EXPECT_EQ(keys(t),
            (std::vector<std::string>{"A", "B", "C", "D", "E", "F", "G"}));
}

TEST(BST, DuplicateIsRejected) {
  Tree t;
  insertAll(t, {"B", "A", "B"});
  EXPECT_EQ(keys(t), (std::vector<std::string>{"A", "B"}));
}

TEST(BST, RemoveNodeWithTwoChildren) {
  Tree t;
  insertAll(t, {"B", "A", "D", "C", "E"});
  t.Remove("D");
  EXPECT_EQ(keys(t), (std::vector<std::string>{"A", "B", "C", "E"}));
  EXPECT_FALSE(t.Search("D").has_value());
}

TEST(BST, RemoveRoot) { // the original use-after-free bug
  Tree t;
  insertAll(t, {"B", "A", "C"});
  t.Remove("B");
  EXPECT_EQ(keys(t), (std::vector<std::string>{"A", "C"}));
}
