// Unit tests for AVLTree<T> (avl.h).
//
// Most tests use a tiny TestItem type instead of Course, so each test controls
// its own keys and doesn't depend on CSV parsing. One test at the end runs the
// tree with the real Course type.
//
// IsValidAVL() is the core of the suite: it walks the whole tree and checks the
// three AVL invariants (BST ordering, correct stored heights, |balance| <= 1).
// Many tests call it after every single operation, so a bug shows up at the
// exact step that caused it.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "avl.h"
#include "course.h"

namespace {

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

// Minimal item type: the tree only needs getKey().
struct TestItem {
  std::string id;
  std::string payload;
  const std::string &getKey() const { return id; }
};

using Tree = AVLTree<TestItem>;
using Node = AVLNode<TestItem>;

// Zero-padded keys so string order matches numeric order (K0002 < K0010).
std::string Key(int i) {
  std::ostringstream out;
  out << 'K' << std::setw(4) << std::setfill('0') << i;
  return out.str();
}

TestItem Item(int i) { return {Key(i), "payload " + std::to_string(i)}; }

void InsertAll(Tree &t, const std::vector<int> &keys) {
  for (int k : keys)
    t.Insert(Item(k));
}

std::vector<int> Range(int first, int last) { // inclusive
  std::vector<int> v;
  for (int i = first; i <= last; ++i)
    v.push_back(i);
  return v;
}

// Checks ordering, stored heights and balance below n. Returns the real
// height. On the first problem found, writes a message to err.
int Check(const Node *n, const std::string *lo, const std::string *hi,
          std::string &err) {
  if (n == nullptr || !err.empty())
    return 0;

  const std::string &k = n->getKey();
  if ((lo && !(*lo < k)) || (hi && !(k < *hi))) {
    err = "BST ordering violated at " + k;
    return 0;
  }

  int l = Check(n->left.get(), lo, &k, err);
  int r = Check(n->right.get(), &k, hi, err);
  if (!err.empty())
    return 0;

  int h = 1 + std::max(l, r);
  if (n->height != h)
    err = "stored height " + std::to_string(n->height) + " but actual height " +
          std::to_string(h) + " at " + k;
  else if (std::abs(l - r) > 1)
    err = "balance factor " + std::to_string(l - r) + " at " + k;
  return h;
}

::testing::AssertionResult IsValidAVL(const Tree &t) {
  std::string err;
  Check(t.Root(), nullptr, nullptr, err);
  if (err.empty())
    return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure() << err;
}

int Height(const Tree &t) { return t.Root() ? t.Root()->height : 0; }

// Worst-case AVL height for n nodes: h < 1.4405 * log2(n + 2) - 0.3277.
int MaxAVLHeight(int n) {
  return static_cast<int>(1.4405 * std::log2(n + 2.0) - 0.3277);
}

std::vector<std::string> InOrderKeys(const Tree &t) {
  std::vector<std::string> keys;
  t.InOrder([&](const TestItem &i) { keys.push_back(i.id); });
  return keys;
}

std::vector<std::string> PreOrderKeys(const Tree &t) {
  std::vector<std::string> keys;
  t.PreOrder([&](const TestItem &i) { keys.push_back(i.id); });
  return keys;
}

std::vector<std::string> PostOrderKeys(const Tree &t) {
  std::vector<std::string> keys;
  t.PostOrder([&](const TestItem &i) { keys.push_back(i.id); });
  return keys;
}

std::vector<std::string> Keys(const std::vector<int> &ids) {
  std::vector<std::string> keys;
  for (int i : ids)
    keys.push_back(Key(i));
  return keys;
}

/* After inserting 10, 20, 30 in any order, the result must be
 *       20
 *      /  \
 *    10    30
 */
void ExpectBalancedTriple(const Tree &t) {
  const Node *root = t.Root();
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->getKey(), Key(20));
  EXPECT_EQ(root->height, 2);

  ASSERT_NE(root->left, nullptr);
  ASSERT_NE(root->right, nullptr);
  EXPECT_EQ(root->left->getKey(), Key(10));
  EXPECT_EQ(root->right->getKey(), Key(30));
  EXPECT_EQ(root->left->height, 1);
  EXPECT_EQ(root->right->height, 1);

  EXPECT_TRUE(IsValidAVL(t));
}

} // namespace

// ---------------------------------------------------------------------------
// Empty tree
// ---------------------------------------------------------------------------

TEST(AVLEmpty, NewTreeHasNoRoot) {
  Tree t;
  EXPECT_EQ(t.Root(), nullptr);
  EXPECT_EQ(Height(t), 0);
}

TEST(AVLEmpty, SearchFindsNothing) {
  Tree t;
  EXPECT_FALSE(t.Search(Key(1)).has_value());
}

TEST(AVLEmpty, TraversalsVisitNothing) {
  Tree t;
  EXPECT_TRUE(InOrderKeys(t).empty());
  EXPECT_TRUE(PreOrderKeys(t).empty());
  EXPECT_TRUE(PostOrderKeys(t).empty());
}

TEST(AVLEmpty, RemoveAndClearAreSafe) {
  Tree t;
  t.Remove(Key(1));
  t.Clear();
  EXPECT_EQ(t.Root(), nullptr);
}

// ---------------------------------------------------------------------------
// Insert and rotations
// ---------------------------------------------------------------------------

TEST(AVLInsert, FirstItemBecomesLeafRoot) {
  Tree t;
  t.Insert(Item(1));

  ASSERT_NE(t.Root(), nullptr);
  EXPECT_EQ(t.Root()->getKey(), Key(1));
  EXPECT_EQ(t.Root()->height, 1);
  EXPECT_EQ(t.Root()->left, nullptr);
  EXPECT_EQ(t.Root()->right, nullptr);
}

TEST(AVLInsert, LeftLeftCaseRotatesRight) {
  Tree t;
  InsertAll(t, {30, 20, 10});
  ExpectBalancedTriple(t);
}

TEST(AVLInsert, RightRightCaseRotatesLeft) {
  Tree t;
  InsertAll(t, {10, 20, 30});
  ExpectBalancedTriple(t);
}

TEST(AVLInsert, LeftRightCaseDoubleRotates) {
  Tree t;
  InsertAll(t, {30, 10, 20});
  ExpectBalancedTriple(t);
}

TEST(AVLInsert, RightLeftCaseDoubleRotates) {
  Tree t;
  InsertAll(t, {10, 30, 20});
  ExpectBalancedTriple(t);
}

/* Inserting 6 rotates left at 2. Node 3 (the "B" subtree) must move from
 * 4's left side to become 2's right child.
 *         4
 *       /   \
 *      2     5
 *     / \     \
 *    1   3     6
 */
TEST(AVLInsert, RotationReattachesInnerSubtree) {
  Tree t;
  InsertAll(t, Range(1, 6));

  const Node *root = t.Root();
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->getKey(), Key(4));
  ASSERT_NE(root->left, nullptr);
  EXPECT_EQ(root->left->getKey(), Key(2));
  ASSERT_NE(root->left->right, nullptr);
  EXPECT_EQ(root->left->right->getKey(), Key(3));
  ASSERT_NE(root->right, nullptr);
  EXPECT_EQ(root->right->getKey(), Key(5));
  ASSERT_NE(root->right->right, nullptr);
  EXPECT_EQ(root->right->right->getKey(), Key(6));
  EXPECT_TRUE(IsValidAVL(t));
}

TEST(AVLInsert, SortedOneToSevenBuildsPerfectTree) {
  Tree t;
  InsertAll(t, Range(1, 7));

  EXPECT_EQ(Height(t), 3);
  EXPECT_EQ(PreOrderKeys(t), Keys({4, 2, 1, 3, 6, 5, 7}));
  EXPECT_TRUE(IsValidAVL(t));
}

// The motivating case: sorted input that turns a plain BST into a list.
TEST(AVLInsert, SortedAscendingStaysBalancedAtEveryStep) {
  Tree t;
  for (int i = 1; i <= 1000; ++i) {
    t.Insert(Item(i));
    ASSERT_TRUE(IsValidAVL(t)) << "after inserting " << Key(i);
  }
  EXPECT_LE(Height(t), MaxAVLHeight(1000));
  EXPECT_EQ(InOrderKeys(t).size(), 1000u);
}

TEST(AVLInsert, SortedDescendingStaysBalancedAtEveryStep) {
  Tree t;
  for (int i = 1000; i >= 1; --i) {
    t.Insert(Item(i));
    ASSERT_TRUE(IsValidAVL(t)) << "after inserting " << Key(i);
  }
  EXPECT_LE(Height(t), MaxAVLHeight(1000));
  EXPECT_EQ(InOrderKeys(t).size(), 1000u);
}

// 2^k - 1 keys inserted in sorted order produce a perfect tree of height k.
TEST(AVLInsert, SortedPowerOfTwoMinusOneIsPerfect) {
  Tree t;
  InsertAll(t, Range(1, 1023));
  EXPECT_EQ(Height(t), 10);
  EXPECT_TRUE(IsValidAVL(t));
}

TEST(AVLInsert, DuplicateIsRejectedAndOriginalKept) {
  Tree t;
  t.Insert({Key(5), "first"});

  ::testing::internal::CaptureStdout();
  t.Insert({Key(5), "second"});
  std::string output = ::testing::internal::GetCapturedStdout();

  EXPECT_NE(output.find("already exists"), std::string::npos);
  EXPECT_EQ(InOrderKeys(t).size(), 1u);

  auto found = t.Search(Key(5));
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->payload, "first");
}

TEST(AVLInsert, DuplicateDeepInTreeLeavesTreeUnchanged) {
  Tree t;
  InsertAll(t, Range(1, 20));
  auto before = PreOrderKeys(t);

  ::testing::internal::CaptureStdout();
  t.Insert(Item(7));
  ::testing::internal::GetCapturedStdout();

  EXPECT_EQ(PreOrderKeys(t), before);
  EXPECT_TRUE(IsValidAVL(t));
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

TEST(AVLSearch, FindsEveryKeyWithItsData) {
  std::vector<int> ids = Range(1, 100);
  std::shuffle(ids.begin(), ids.end(), std::mt19937(7));

  Tree t;
  InsertAll(t, ids);

  for (int i = 1; i <= 100; ++i) {
    auto found = t.Search(Key(i));
    ASSERT_TRUE(found.has_value()) << Key(i);
    EXPECT_EQ(found->id, Key(i));
    EXPECT_EQ(found->payload, "payload " + std::to_string(i));
  }
}

TEST(AVLSearch, MissingKeysReturnNullopt) {
  Tree t;
  for (int i = 2; i <= 100; i += 2) // even keys only
    t.Insert(Item(i));

  EXPECT_FALSE(t.Search(Key(0)).has_value());   // below the minimum
  EXPECT_FALSE(t.Search(Key(101)).has_value()); // above the maximum
  for (int i = 1; i < 100; i += 2)              // gaps between keys
    EXPECT_FALSE(t.Search(Key(i)).has_value()) << Key(i);
}

TEST(AVLSearch, KeysAreCaseSensitive) {
  Tree t;
  t.Insert({"CSCI100", "x"});
  EXPECT_TRUE(t.Search("CSCI100").has_value());
  EXPECT_FALSE(t.Search("csci100").has_value());
}

TEST(AVLSearch, ReturnsCopyNotReference) {
  Tree t;
  t.Insert({Key(1), "original"});

  auto found = t.Search(Key(1));
  ASSERT_TRUE(found.has_value());
  found->payload = "changed";

  EXPECT_EQ(t.Search(Key(1))->payload, "original");
}

// ---------------------------------------------------------------------------
// Traversals
// ---------------------------------------------------------------------------

TEST(AVLTraversal, InOrderIsSortedForRandomInput) {
  std::vector<int> ids = Range(1, 500);
  std::shuffle(ids.begin(), ids.end(), std::mt19937(11));

  Tree t;
  InsertAll(t, ids);

  EXPECT_EQ(InOrderKeys(t), Keys(Range(1, 500)));
}

TEST(AVLTraversal, PreAndPostOrderMatchKnownShape) {
  Tree t;
  InsertAll(t, Range(1, 7)); // perfect tree rooted at 4

  EXPECT_EQ(PreOrderKeys(t), Keys({4, 2, 1, 3, 6, 5, 7}));
  EXPECT_EQ(PostOrderKeys(t), Keys({1, 3, 2, 5, 7, 6, 4}));
}

// ---------------------------------------------------------------------------
// Remove
// ---------------------------------------------------------------------------

TEST(AVLRemove, LeafNode) {
  Tree t;
  InsertAll(t, Range(1, 7));
  t.Remove(Key(1));

  EXPECT_EQ(InOrderKeys(t), Keys({2, 3, 4, 5, 6, 7}));
  EXPECT_FALSE(t.Search(Key(1)).has_value());
  EXPECT_TRUE(IsValidAVL(t));
}

TEST(AVLRemove, NodeWithOneChild) {
  Tree t;
  InsertAll(t, {2, 1, 3, 4}); // 3 has a single right child, 4
  t.Remove(Key(3));

  ASSERT_NE(t.Root(), nullptr);
  ASSERT_NE(t.Root()->right, nullptr);
  EXPECT_EQ(t.Root()->right->getKey(), Key(4));
  EXPECT_EQ(InOrderKeys(t), Keys({1, 2, 4}));
  EXPECT_TRUE(IsValidAVL(t));
}

TEST(AVLRemove, NodeWithTwoChildrenTakesSuccessor) {
  Tree t;
  InsertAll(t, Range(1, 7));
  t.Remove(Key(2)); // children 1 and 3; successor is 3

  ASSERT_NE(t.Root()->left, nullptr);
  EXPECT_EQ(t.Root()->left->getKey(), Key(3));
  EXPECT_EQ(InOrderKeys(t), Keys({1, 3, 4, 5, 6, 7}));
  EXPECT_TRUE(IsValidAVL(t));
}

TEST(AVLRemove, RootNode) {
  Tree t;
  InsertAll(t, Range(1, 7));
  t.Remove(Key(4));

  ASSERT_NE(t.Root(), nullptr);
  EXPECT_EQ(t.Root()->getKey(), Key(5)); // in-order successor of 4
  EXPECT_EQ(InOrderKeys(t), Keys({1, 2, 3, 5, 6, 7}));
  EXPECT_TRUE(IsValidAVL(t));
}

/* Root 4's successor is 6, which has its own right child (7).
 *        4                 6
 *      /   \             /   \
 *     2     8    -->    2     8
 *    / \   / \         / \   / \
 *   1  3  6   9       1  3  7   9
 *          \
 *           7
 */
TEST(AVLRemove, SuccessorWithRightChild) {
  Tree t;
  InsertAll(t, {4, 2, 8, 1, 3, 6, 9, 7});
  ASSERT_TRUE(IsValidAVL(t));

  t.Remove(Key(4));

  EXPECT_EQ(t.Root()->getKey(), Key(6));
  ASSERT_NE(t.Root()->right->left, nullptr);
  EXPECT_EQ(t.Root()->right->left->getKey(), Key(7));
  EXPECT_EQ(InOrderKeys(t), Keys({1, 2, 3, 6, 7, 8, 9}));
  EXPECT_TRUE(IsValidAVL(t));
}

TEST(AVLRemove, MissingKeyChangesNothing) {
  Tree t;
  InsertAll(t, Range(1, 7));
  auto before = PreOrderKeys(t);

  t.Remove(Key(99));

  EXPECT_EQ(PreOrderKeys(t), before);
  EXPECT_TRUE(IsValidAVL(t));
}

// Removing 1 leaves 2 right-heavy (-2) with right child 3 leaning right:
// single left rotation, so 3 becomes the root.
TEST(AVLRemove, TriggersSingleRotation) {
  Tree t;
  InsertAll(t, {2, 1, 3, 4});
  t.Remove(Key(1));

  EXPECT_EQ(t.Root()->getKey(), Key(3));
  EXPECT_EQ(PreOrderKeys(t), Keys({3, 2, 4}));
  EXPECT_TRUE(IsValidAVL(t));
}

// Removing 1 leaves 2 right-heavy with right child 4 leaning left:
// right-left double rotation, so 3 becomes the root.
TEST(AVLRemove, TriggersDoubleRotation) {
  Tree t;
  InsertAll(t, {2, 1, 4, 3});
  t.Remove(Key(1));

  EXPECT_EQ(t.Root()->getKey(), Key(3));
  EXPECT_EQ(PreOrderKeys(t), Keys({3, 2, 4}));
  EXPECT_TRUE(IsValidAVL(t));
}

// The case only deletion produces: the tall child has balance 0. A single
// rotation is correct here; a double rotation would leave the tree unbalanced.
TEST(AVLRemove, BalancedTallChildUsesSingleRotation) {
  Tree t;
  InsertAll(t, {2, 1, 4, 3, 5}); // 4 has two children, balance 0
  t.Remove(Key(1));

  EXPECT_EQ(t.Root()->getKey(), Key(4));
  EXPECT_EQ(PreOrderKeys(t), Keys({4, 2, 3, 5}));
  EXPECT_EQ(Height(t), 3);
  EXPECT_TRUE(IsValidAVL(t));
}

TEST(AVLRemove, RemovingEverythingEmptiesTree) {
  std::vector<int> ids = Range(1, 300);
  Tree t;
  InsertAll(t, ids);

  std::shuffle(ids.begin(), ids.end(), std::mt19937(3));
  for (int id : ids) {
    t.Remove(Key(id));
    ASSERT_TRUE(IsValidAVL(t)) << "after removing " << Key(id);
    ASSERT_FALSE(t.Search(Key(id)).has_value());
  }
  EXPECT_EQ(t.Root(), nullptr);

  t.Insert(Item(1)); // still usable
  EXPECT_TRUE(t.Search(Key(1)).has_value());
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

TEST(AVLClear, EmptiesTree) {
  Tree t;
  InsertAll(t, Range(1, 100));
  t.Clear();

  EXPECT_EQ(t.Root(), nullptr);
  EXPECT_FALSE(t.Search(Key(50)).has_value());
  EXPECT_TRUE(InOrderKeys(t).empty());
}

TEST(AVLClear, TreeIsReusableAfterClear) {
  Tree t;
  InsertAll(t, Range(1, 100));
  t.Clear();
  InsertAll(t, Range(200, 210));

  EXPECT_EQ(InOrderKeys(t), Keys(Range(200, 210)));
  EXPECT_TRUE(IsValidAVL(t));
}

TEST(AVLClear, ClearingTwiceIsSafe) {
  Tree t;
  InsertAll(t, Range(1, 10));
  t.Clear();
  t.Clear();
  EXPECT_EQ(t.Root(), nullptr);
}

TEST(AVLClear, HandlesLargeTree) {
  Tree t;
  InsertAll(t, Range(1, 100000));
  t.Clear();
  EXPECT_EQ(t.Root(), nullptr);
}

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------

TEST(AVLMove, ConstructorTransfersWholeTree) {
  Tree a;
  InsertAll(a, Range(1, 50));

  Tree b(std::move(a));

  EXPECT_EQ(InOrderKeys(b), Keys(Range(1, 50)));
  EXPECT_TRUE(IsValidAVL(b));
  EXPECT_EQ(a.Root(), nullptr); // moved-from tree is empty but usable
  a.Insert(Item(1));
  EXPECT_TRUE(a.Search(Key(1)).has_value());
}

TEST(AVLMove, AssignmentReplacesContents) {
  Tree a;
  InsertAll(a, Range(1, 10));
  Tree b;
  InsertAll(b, Range(100, 120));

  b = std::move(a);

  EXPECT_EQ(InOrderKeys(b), Keys(Range(1, 10)));
  EXPECT_FALSE(b.Search(Key(100)).has_value());
  EXPECT_TRUE(IsValidAVL(b));
}

// ---------------------------------------------------------------------------
// Randomized stress test against std::set
// ---------------------------------------------------------------------------

TEST(AVLStress, RandomInsertsAndRemovesMatchStdSet) {
  std::mt19937 rng(42); // fixed seed, so any failure is reproducible

  for (int round = 0; round < 10; ++round) {
    Tree t;
    std::set<std::string> expected;

    for (int op = 0; op < 3000; ++op) {
      int id = static_cast<int>(rng() % 1000);
      std::string key = Key(id);

      if (rng() % 3 != 0) { // ~2/3 inserts, ~1/3 removes
        if (expected.insert(key).second)
          t.Insert(Item(id));
      } else {
        expected.erase(key);
        t.Remove(key);
      }

      ASSERT_TRUE(IsValidAVL(t))
          << "round " << round << ", operation " << op << ", key " << key;
    }

    std::vector<std::string> expectedKeys(expected.begin(), expected.end());
    EXPECT_EQ(InOrderKeys(t), expectedKeys);
    for (const auto &key : expected)
      EXPECT_TRUE(t.Search(key).has_value()) << key;
    EXPECT_LE(Height(t), MaxAVLHeight(static_cast<int>(expected.size())));
  }
}

// ---------------------------------------------------------------------------
// With the real Course type
// ---------------------------------------------------------------------------

TEST(AVLWithCourse, SortedCatalogStaysBalanced) {
  AVLTree<Course> t;
  std::vector<std::string> ids = {"CSCI100", "CSCI101", "CSCI200", "CSCI300",
                                  "CSCI301", "CSCI350", "CSCI400", "MATH201"};
  for (const auto &id : ids) {
    std::string courseId = id;
    std::string title = "Title for " + id;
    std::vector<std::string> prereqs;
    Course course(courseId, title, prereqs);
    t.Insert(course);
  }

  std::vector<std::string> visited;
  t.InOrder([&](const Course &c) { visited.push_back(c.getKey()); });
  EXPECT_EQ(visited, ids);

  ASSERT_NE(t.Root(), nullptr);
  EXPECT_LE(t.Root()->height, 4); // a plain BST would be 8 tall here

  auto found = t.Search("CSCI300");
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->getKey(), "CSCI300");
}
