// Unit tests for BinarySearchTree<T> (bst.h).
//
// Same structure as avl_test.cpp: most tests use a tiny TestItem type so each
// test controls its own keys, and a few at the end use the real Course type.
//
// The plain BST has no balancing, so the shape of the tree depends entirely on
// insertion order. Several tests pin that behavior down on purpose (the root
// never changes, sorted input becomes a chain). These are the baseline the AVL
// tree is compared against, so they belong in the suite too.

#include <gtest/gtest.h>

#include <algorithm>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "bst.h"
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

using Tree = BinarySearchTree<TestItem>;
using TreeNode = Node<TestItem>;

// Zero-padded keys so string order matches numeric order (K0002 < K0010).
std::string Key(int i) {
  std::ostringstream out;
  out << 'K' << std::setw(5) << std::setfill('0') << i;
  return out.str();
}

TestItem Item(int i) { return {Key(i), "payload " + std::to_string(i)}; }

void InsertAll(Tree &t, const std::vector<int> &ids) {
  for (int id : ids)
    t.Insert(Item(id));
}

std::vector<int> Range(int first, int last) { // inclusive
  std::vector<int> v;
  for (int i = first; i <= last; ++i)
    v.push_back(i);
  return v;
}

std::vector<std::string> Keys(const std::vector<int> &ids) {
  std::vector<std::string> keys;
  for (int id : ids)
    keys.push_back(Key(id));
  return keys;
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

// Height measured with an explicit stack, so it also works on a chain
// thousands of nodes deep.
int Height(const Tree &t) {
  int best = 0;
  std::vector<std::pair<const TreeNode *, int>> stack;
  if (t.Root())
    stack.push_back({t.Root(), 1});
  while (!stack.empty()) {
    auto [n, depth] = stack.back();
    stack.pop_back();
    best = std::max(best, depth);
    if (n->left)
      stack.push_back({n->left.get(), depth + 1});
    if (n->right)
      stack.push_back({n->right.get(), depth + 1});
  }
  return best;
}

// Checks the BST ordering rule across the whole tree: every key is greater
// than everything in its left subtree and less than everything in its right
// subtree (not just its direct children).
::testing::AssertionResult IsValidBST(const Tree &t) {
  struct Frame {
    const TreeNode *node;
    const std::string *lo;
    const std::string *hi;
  };
  std::vector<Frame> stack;
  if (t.Root())
    stack.push_back({t.Root(), nullptr, nullptr});

  while (!stack.empty()) {
    Frame f = stack.back();
    stack.pop_back();
    const std::string &k = f.node->getKey();
    if ((f.lo && !(*f.lo < k)) || (f.hi && !(k < *f.hi)))
      return ::testing::AssertionFailure() << "BST ordering violated at " << k;
    if (f.node->left)
      stack.push_back({f.node->left.get(), f.lo, &k});
    if (f.node->right)
      stack.push_back({f.node->right.get(), &k, f.hi});
  }
  return ::testing::AssertionSuccess();
}

// Inserts while discarding the "already exists" message, for tests that
// insert duplicates on purpose but aren't checking the message.
void InsertQuietly(Tree &t, const TestItem &item) {
  ::testing::internal::CaptureStdout();
  t.Insert(item);
  ::testing::internal::GetCapturedStdout();
}

/* Shared shape for traversal and remove tests:
 *           40
 *         /    \
 *       20      60
 *      /  \    /  \
 *    10   30  50   70
 */
void BuildSevenNodeTree(Tree &t) { InsertAll(t, {40, 20, 60, 10, 30, 50, 70}); }

} // namespace

// ---------------------------------------------------------------------------
// Compile-time checks: the memory fix (no copies, moves allowed)
// ---------------------------------------------------------------------------

static_assert(!std::is_copy_constructible_v<Tree>, "BST must not be copyable");
static_assert(!std::is_copy_assignable_v<Tree>,
              "BST must not be copy-assignable");
static_assert(std::is_move_constructible_v<Tree>, "BST must be movable");
static_assert(std::is_move_assignable_v<Tree>, "BST must be move-assignable");

// ---------------------------------------------------------------------------
// Empty tree
// ---------------------------------------------------------------------------

TEST(BSTEmpty, NewTreeHasNoRoot) {
  Tree t;
  EXPECT_EQ(t.Root(), nullptr);
  EXPECT_EQ(Height(t), 0);
}

TEST(BSTEmpty, SearchFindsNothing) {
  Tree t;
  EXPECT_FALSE(t.Search(Key(1)).has_value());
}

// The original "print before load" crash: traversing an empty tree.
TEST(BSTEmpty, TraversalsVisitNothing) {
  Tree t;
  EXPECT_TRUE(InOrderKeys(t).empty());
  EXPECT_TRUE(PreOrderKeys(t).empty());
  EXPECT_TRUE(PostOrderKeys(t).empty());
}

TEST(BSTEmpty, RemoveAndClearAreSafe) {
  Tree t;
  t.Remove(Key(1));
  t.Clear();
  EXPECT_EQ(t.Root(), nullptr);
}

// ---------------------------------------------------------------------------
// Insert
// ---------------------------------------------------------------------------

TEST(BSTInsert, FirstItemBecomesRoot) {
  Tree t;
  t.Insert(Item(1));

  ASSERT_NE(t.Root(), nullptr);
  EXPECT_EQ(t.Root()->getKey(), Key(1));
  EXPECT_EQ(t.Root()->left, nullptr);
  EXPECT_EQ(t.Root()->right, nullptr);
}

TEST(BSTInsert, SmallerGoesLeftLargerGoesRight) {
  Tree t;
  InsertAll(t, {20, 10, 30});

  ASSERT_NE(t.Root()->left, nullptr);
  ASSERT_NE(t.Root()->right, nullptr);
  EXPECT_EQ(t.Root()->left->getKey(), Key(10));
  EXPECT_EQ(t.Root()->right->getKey(), Key(30));
}

// No rebalancing: the first key stays the root no matter what follows.
TEST(BSTInsert, RootNeverChanges) {
  Tree t;
  t.Insert(Item(50));
  for (int i = 1; i <= 100; ++i) {
    if (i == 50)
      continue;
    t.Insert(Item(i));
    ASSERT_EQ(t.Root()->getKey(), Key(50));
  }
}

// The weakness the AVL tree fixes: sorted input makes a linked list.
TEST(BSTInsert, SortedAscendingBecomesRightChain) {
  Tree t;
  InsertAll(t, Range(1, 1000));

  EXPECT_EQ(Height(t), 1000);
  for (const TreeNode *n = t.Root(); n; n = n->right.get())
    ASSERT_EQ(n->left, nullptr) << "unexpected left child at " << n->getKey();
  EXPECT_TRUE(IsValidBST(t));
}

TEST(BSTInsert, SortedDescendingBecomesLeftChain) {
  Tree t;
  for (int i = 1000; i >= 1; --i)
    t.Insert(Item(i));

  EXPECT_EQ(Height(t), 1000);
  for (const TreeNode *n = t.Root(); n; n = n->left.get())
    ASSERT_EQ(n->right, nullptr) << "unexpected right child at " << n->getKey();
  EXPECT_TRUE(IsValidBST(t));
}

TEST(BSTInsert, RandomInputKeepsOrdering) {
  std::vector<int> ids = Range(1, 1000);
  std::shuffle(ids.begin(), ids.end(), std::mt19937(5));

  Tree t;
  for (int id : ids) {
    t.Insert(Item(id));
    ASSERT_TRUE(IsValidBST(t)) << "after inserting " << Key(id);
  }
  EXPECT_EQ(InOrderKeys(t), Keys(Range(1, 1000)));
}

TEST(BSTInsert, DuplicateIsRejectedAndOriginalKept) {
  Tree t;
  t.Insert({Key(5), "first"});

  ::testing::internal::CaptureStdout();
  t.Insert({Key(5), "second"});
  std::string output = ::testing::internal::GetCapturedStdout();

  EXPECT_NE(output.find("already exists"), std::string::npos);
  EXPECT_EQ(InOrderKeys(t), Keys({5}));
  EXPECT_EQ(t.Search(Key(5))->payload, "first");
}

TEST(BSTInsert, DuplicateDeepInTreeLeavesTreeUnchanged) {
  Tree t;
  BuildSevenNodeTree(t);
  auto before = PreOrderKeys(t);

  InsertQuietly(t, Item(30)); // a leaf two levels down
  InsertQuietly(t, Item(40)); // the root

  EXPECT_EQ(PreOrderKeys(t), before);
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

TEST(BSTSearch, FindsEveryKeyWithItsData) {
  std::vector<int> ids = Range(1, 200);
  std::shuffle(ids.begin(), ids.end(), std::mt19937(7));

  Tree t;
  InsertAll(t, ids);

  for (int i = 1; i <= 200; ++i) {
    auto found = t.Search(Key(i));
    ASSERT_TRUE(found.has_value()) << Key(i);
    EXPECT_EQ(found->id, Key(i));
    EXPECT_EQ(found->payload, "payload " + std::to_string(i));
  }
}

TEST(BSTSearch, MissingKeysReturnNullopt) {
  Tree t;
  for (int i = 2; i <= 100; i += 2) // even keys only
    t.Insert(Item(i));

  EXPECT_FALSE(t.Search(Key(0)).has_value());   // below the minimum
  EXPECT_FALSE(t.Search(Key(101)).has_value()); // above the maximum
  for (int i = 1; i < 100; i += 2)              // gaps between keys
    EXPECT_FALSE(t.Search(Key(i)).has_value()) << Key(i);
}

TEST(BSTSearch, FindsEndOfLongChain) {
  Tree t;
  InsertAll(t, Range(1, 1000)); // degenerate: Search must walk all of it
  EXPECT_TRUE(t.Search(Key(1000)).has_value());
  EXPECT_FALSE(t.Search(Key(1001)).has_value());
}

TEST(BSTSearch, KeysAreCaseSensitive) {
  Tree t;
  t.Insert({"CSCI100", "x"});
  EXPECT_TRUE(t.Search("CSCI100").has_value());
  EXPECT_FALSE(t.Search("csci100").has_value());
}

TEST(BSTSearch, ReturnsCopyNotReference) {
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

TEST(BSTTraversal, InOrderIsSorted) {
  Tree t;
  BuildSevenNodeTree(t);
  EXPECT_EQ(InOrderKeys(t), Keys({10, 20, 30, 40, 50, 60, 70}));
}

TEST(BSTTraversal, PreOrderVisitsParentFirst) {
  Tree t;
  BuildSevenNodeTree(t);
  EXPECT_EQ(PreOrderKeys(t), Keys({40, 20, 10, 30, 60, 50, 70}));
}

TEST(BSTTraversal, PostOrderVisitsParentLast) {
  Tree t;
  BuildSevenNodeTree(t);
  EXPECT_EQ(PostOrderKeys(t), Keys({10, 30, 20, 50, 70, 60, 40}));
}

// ---------------------------------------------------------------------------
// Remove
// ---------------------------------------------------------------------------

TEST(BSTRemove, LeafNode) {
  Tree t;
  BuildSevenNodeTree(t);
  t.Remove(Key(10));

  EXPECT_EQ(t.Root()->left->left, nullptr);
  EXPECT_EQ(InOrderKeys(t), Keys({20, 30, 40, 50, 60, 70}));
  EXPECT_TRUE(IsValidBST(t));
}

TEST(BSTRemove, NodeWithOnlyLeftChild) {
  Tree t;
  InsertAll(t, {40, 20, 10}); // 20 has only a left child
  t.Remove(Key(20));

  ASSERT_NE(t.Root()->left, nullptr);
  EXPECT_EQ(t.Root()->left->getKey(), Key(10));
  EXPECT_EQ(InOrderKeys(t), Keys({10, 40}));
}

TEST(BSTRemove, NodeWithOnlyRightChild) {
  Tree t;
  InsertAll(t, {40, 20, 30}); // 20 has only a right child
  t.Remove(Key(20));

  ASSERT_NE(t.Root()->left, nullptr);
  EXPECT_EQ(t.Root()->left->getKey(), Key(30));
  EXPECT_EQ(InOrderKeys(t), Keys({30, 40}));
}

TEST(BSTRemove, NodeWithTwoChildrenTakesSuccessor) {
  Tree t;
  BuildSevenNodeTree(t);
  t.Remove(Key(60)); // children 50 and 70; successor is 70

  ASSERT_NE(t.Root()->right, nullptr);
  EXPECT_EQ(t.Root()->right->getKey(), Key(70));
  EXPECT_EQ(InOrderKeys(t), Keys({10, 20, 30, 40, 50, 70}));
  EXPECT_TRUE(IsValidBST(t));
}

/* The successor (50) has its own right child (55), which must be kept.
 *        40                50
 *       /  \              /  \
 *     20    60    -->   20    60
 *          /  \              /  \
 *        50    70          55    70
 *          \
 *           55
 */
TEST(BSTRemove, SuccessorWithRightChild) {
  Tree t;
  InsertAll(t, {40, 20, 60, 50, 70, 55});
  t.Remove(Key(40));

  EXPECT_EQ(t.Root()->getKey(), Key(50));
  ASSERT_NE(t.Root()->right->left, nullptr);
  EXPECT_EQ(t.Root()->right->left->getKey(), Key(55));
  EXPECT_EQ(InOrderKeys(t), Keys({20, 50, 55, 60, 70}));
  EXPECT_TRUE(IsValidBST(t));
}

// The original use-after-free bug: Remove() ignored the new subtree root.
TEST(BSTRemove, OnlyNodeLeavesEmptyTree) {
  Tree t;
  t.Insert(Item(1));
  t.Remove(Key(1));

  EXPECT_EQ(t.Root(), nullptr);
  EXPECT_FALSE(t.Search(Key(1)).has_value());
}

TEST(BSTRemove, RootWithOneChild) {
  Tree t;
  InsertAll(t, {10, 20, 30}); // chain: 10 -> 20 -> 30
  t.Remove(Key(10));

  ASSERT_NE(t.Root(), nullptr);
  EXPECT_EQ(t.Root()->getKey(), Key(20));
  EXPECT_EQ(InOrderKeys(t), Keys({20, 30}));
}

TEST(BSTRemove, RootWithTwoChildren) {
  Tree t;
  BuildSevenNodeTree(t);
  t.Remove(Key(40));

  ASSERT_NE(t.Root(), nullptr);
  EXPECT_EQ(t.Root()->getKey(), Key(50)); // in-order successor of 40
  EXPECT_EQ(InOrderKeys(t), Keys({10, 20, 30, 50, 60, 70}));
  EXPECT_TRUE(IsValidBST(t));
}

TEST(BSTRemove, MissingKeyChangesNothing) {
  Tree t;
  BuildSevenNodeTree(t);
  auto before = PreOrderKeys(t);

  t.Remove(Key(45)); // falls between existing keys
  t.Remove(Key(99)); // larger than everything

  EXPECT_EQ(PreOrderKeys(t), before);
}

TEST(BSTRemove, RemovingEverythingEmptiesTree) {
  std::vector<int> ids = Range(1, 300);
  std::shuffle(ids.begin(), ids.end(), std::mt19937(3));
  Tree t;
  InsertAll(t, ids);

  std::shuffle(ids.begin(), ids.end(), std::mt19937(4));
  for (int id : ids) {
    t.Remove(Key(id));
    ASSERT_TRUE(IsValidBST(t)) << "after removing " << Key(id);
    ASSERT_FALSE(t.Search(Key(id)).has_value());
  }
  EXPECT_EQ(t.Root(), nullptr);

  t.Insert(Item(1)); // still usable
  EXPECT_TRUE(t.Search(Key(1)).has_value());
}

// ---------------------------------------------------------------------------
// Clear and destruction
// ---------------------------------------------------------------------------

TEST(BSTClear, EmptiesTree) {
  Tree t;
  BuildSevenNodeTree(t);
  t.Clear();

  EXPECT_EQ(t.Root(), nullptr);
  EXPECT_FALSE(t.Search(Key(40)).has_value());
  EXPECT_TRUE(InOrderKeys(t).empty());
}

TEST(BSTClear, TreeIsReusableAfterClear) {
  Tree t;
  BuildSevenNodeTree(t);
  t.Clear();
  InsertAll(t, {2, 1, 3});

  EXPECT_EQ(InOrderKeys(t), Keys({1, 2, 3}));
  EXPECT_EQ(t.Root()->getKey(), Key(2)); // new insertion order, new root
}

TEST(BSTClear, ClearingTwiceIsSafe) {
  Tree t;
  BuildSevenNodeTree(t);
  t.Clear();
  t.Clear();
  EXPECT_EQ(t.Root(), nullptr);
}

// Clear() is iterative, so a deep chain is no harder than a balanced tree.
// (Sorted inserts into a BST are O(n^2), which keeps n modest here.)
TEST(BSTClear, HandlesDegenerateChain) {
  Tree t;
  InsertAll(t, Range(1, 2000));
  t.Clear();
  EXPECT_EQ(t.Root(), nullptr);
}

TEST(BSTClear, DestructorFreesDegenerateChain) {
  {
    Tree t;
    InsertAll(t, Range(1, 2000));
  } // ~BinarySearchTree() runs Clear() here; sanitizers report any leak
  SUCCEED();
}

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------

TEST(BSTMove, ConstructorTransfersWholeTree) {
  Tree a;
  BuildSevenNodeTree(a);

  Tree b(std::move(a));

  EXPECT_EQ(InOrderKeys(b), Keys({10, 20, 30, 40, 50, 60, 70}));
  EXPECT_EQ(a.Root(), nullptr); // moved-from tree is empty but usable
  a.Insert(Item(1));
  EXPECT_TRUE(a.Search(Key(1)).has_value());
}

TEST(BSTMove, AssignmentReplacesContents) {
  Tree a;
  BuildSevenNodeTree(a);
  Tree b;
  InsertAll(b, {100, 200});

  b = std::move(a);

  EXPECT_EQ(InOrderKeys(b), Keys({10, 20, 30, 40, 50, 60, 70}));
  EXPECT_FALSE(b.Search(Key(100)).has_value());
}

// ---------------------------------------------------------------------------
// Randomized stress test against std::set
// ---------------------------------------------------------------------------

TEST(BSTStress, RandomInsertsAndRemovesMatchStdSet) {
  std::mt19937 rng(42); // fixed seed, so any failure is reproducible

  for (int round = 0; round < 5; ++round) {
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

      ASSERT_TRUE(IsValidBST(t))
          << "round " << round << ", operation " << op << ", key " << key;
    }

    std::vector<std::string> expectedKeys(expected.begin(), expected.end());
    EXPECT_EQ(InOrderKeys(t), expectedKeys);
    for (const auto &key : expected)
      EXPECT_TRUE(t.Search(key).has_value()) << key;
  }
}

// ---------------------------------------------------------------------------
// With the real Course type
// ---------------------------------------------------------------------------

TEST(BSTWithCourse, StoresAndFindsCourses) {
  BinarySearchTree<Course> t;
  for (const char *id : {"CSCI300", "CSCI100", "MATH201", "CSCI200"})
    t.Insert(Course(id, "Title"));

  std::vector<std::string> visited;
  t.InOrder([&](const Course &c) { visited.push_back(c.getKey()); });
  EXPECT_EQ(visited, (std::vector<std::string>{"CSCI100", "CSCI200", "CSCI300",
                                               "MATH201"}));

  auto found = t.Search("CSCI200");
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->getKey(), "CSCI200");
  EXPECT_FALSE(t.Search("CSCI999").has_value());
}

// A sorted catalog (how course files usually arrive) is a chain in a BST.
TEST(BSTWithCourse, SortedCatalogDegenerates) {
  BinarySearchTree<Course> t;
  const std::vector<std::string> ids = {"CSCI100", "CSCI101", "CSCI200",
                                        "CSCI300", "CSCI301", "CSCI350",
                                        "CSCI400", "MATH201"};
  for (const auto &id : ids)
    t.Insert(Course(id, "Title"));

  int depth = 0;
  for (const auto *n = t.Root(); n; n = n->right.get())
    ++depth;
  EXPECT_EQ(depth, static_cast<int>(ids.size())); // every node is a right child
}
