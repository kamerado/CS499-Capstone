#ifndef BST_H
#define BST_H

#include <iostream>
#include <memory>
#include <string>

#include "course.h"
#include "csv.h"
#include "helpers.h"

// // Struct course.
// struct Course {
//   std::string courseID;
//   std::string courseTitle;
//   std::vector<std::string> prereq;
//
//   // default constructor.
//   Course() = default;
//
//   // FIXed: (SE)[Structure]: Constructors take strings and vectors by value.
//   use
//   // const&.
//   // Default constructor assigning "\0" just makes an expty string. use
//   default
//   // member initializers instead. constructor with id.
//   explicit Course(const std::string &id) : courseID(id) {}
//
//   // cosntructor with id, title.
//   Course(const std::string &id, const std::string &title)
//       : courseID(id), courseTitle(title) {}
//
//   // constructor with id, title, prereqs.
//   Course(const std::string &id, const std::string &title,
//          const std::vector<std::string> &preReq)
//       : courseID(id), courseTitle(title), prereq(preReq) {}
//
//   // Func to print course data.
//   void print() const {
//     std::cout << "Course ID: " << courseID << "\n"
//               << "Course Title: " << courseTitle << "\n";
//     if (!prereq.empty()) {
//       auto it = prereq.begin();
//       std::cout << "Prerequisites: " << "\n";
//       for (; it < prereq.end(); it++) {
//         std::cout << "    " << *it << "\n";
//       }
//       std::cout << std::endl;
//     }
//   }
//
//   const std::string &getKey() const { return courseID; }
// };

/*
 * Struct Node, using typename T for item.
 */
// FIXED: (SE)[Effeciency]: Key duplicates item.courseID, so each course id is
// stored twice. Derive the key from the item instead.
template <typename T> struct Node {
  T item;
  std::unique_ptr<Node> left = nullptr;
  std::unique_ptr<Node> right = nullptr;

  // default constructor.
  Node() = default;

  // constructor with item
  Node(const T &aItem) : item(aItem) {}

  const std::string &getKey() const { return item.getKey(); }
};

/*
 * Class declaration for BinarySearchTree, using struct Node, and typename T for
 * item type.
 */
// TODO: (ALG)[Effeciency] CourseID are usually sorted, so the BST is basically
// a linked List with O(n) search. Inserting is also O(n), so loading sorted
// data is O(n^2). To fix this, I will replace this with a self balancing AVL
// tree.
//
// FIXED: (SE)[Memory]: Class owns raw pointers but does not delete its copy
// constructor and assisgnment operator. A copy would share nodes and both
// destructors would free them (double free). Fix: Delete them or use
// unique_ptr. This, even though it will be replaced with the AVL tree, is
// critical for for benchmarking the BST against the new AVL tree later.
template <typename T> class BinarySearchTree {

  // private member variables.
private:
  std::unique_ptr<Node<T>> root;

  // Private member methods.
private:
  void addNode(const T &item);
  void inOrder(Node<T> *node, void (*func)(Node<T> *));
  void postOrder(Node<T> *node, void (*func)(Node<T> *));
  void preOrder(Node<T> *node, void (*func)(Node<T> *));
  std::unique_ptr<Node<T>> removeNode(std::unique_ptr<Node<T>> node,
                                      const std::string &courseID);
  Node<T> *findMinNode(Node<T> *);

  // Constructors
public:
  BinarySearchTree() = default;
  virtual ~BinarySearchTree() = default;

  // Ownership is unique: no copies (prevents the shared-node double free),
  // but moves are allowed and transfer the whole tree.
  BinarySearchTree(const BinarySearchTree &) = delete;
  BinarySearchTree &operator=(const BinarySearchTree &) = delete;
  BinarySearchTree(BinarySearchTree &&) = default;
  BinarySearchTree &operator=(BinarySearchTree &&) = default;

  // Public member methods.
public:
  void InOrder(void (*func)(Node<T> *));
  void PostOrder(void (*func)(Node<T> *));
  void PreOrder(void (*func)(Node<T> *));
  void Insert(const T &item);
  void Remove(const std::string &itemID);
  std::optional<T> Search(const std::string &itemID);
};

/**
 * Traverse the tree in order
 */
// TODO: (ALG)[Effeciency]: clear() is UI logic and it should'nt be in the
// data structure logic. Move this into the print screen fucntion before
// printing.
template <typename T>
void BinarySearchTree<T>::InOrder(void (*func)(Node<T> *)) {
  clear();
  inOrder(root.get(), func);
}

/**
 * Traverse the tree in post-order
 */
template <typename T>
void BinarySearchTree<T>::PostOrder(void (*func)(Node<T> *)) {
  postOrder(root.get(), func);
}

/**
 * Traverse the tree in pre-order
 */
template <typename T>
void BinarySearchTree<T>::PreOrder(void (*func)(Node<T> *)) {
  preOrder(root.get(), func);
}

/**
 * Insert an node.
 */
// FIXED: (SE)[Structure]: This uses std::string& and search uses std::string.
// Use const std::string& instead of consistency, and to avoid copies.
template <typename T> void BinarySearchTree<T>::Insert(const T &item) {
  addNode(item);
}

/**
 * Remove a node.
 */
// FIXED: (SE)[Memory]: removeNode() returns the subtree root. but remove()
// ignores it. Removing the root node deletes it while root still points to it,
// (dangling pointer, use after free). Fix: root = removeNode(root, itemID);
template <typename T>
void BinarySearchTree<T>::Remove(const std::string &itemID) {
  root = removeNode(std::move(root), itemID);
}

/**
 * Search for a node.
 */
// FIXED: (SE)[Structure]: returns empty course as not found. We can use an
// optional here instead.
template <typename T>
std::optional<T> BinarySearchTree<T>::Search(const std::string &itemID) {
  // set current node equal to root
  Node<T> *curr = root.get();

  // keep looping downwards until bottom reached or match
  while (curr != nullptr) {
    // if match found, return
    if (itemID == curr->getKey()) {
      return curr->item;
      // if smaller than current node then traverse left
    } else if (itemID < curr->getKey()) {
      curr = curr->left.get();

      // else larger so traverse right
    } else {
      curr = curr->right.get();
    }
  }

  return std::nullopt;
}

/*
 * Function to add node to bst.
 */
// FIXED: (ALG)[Correctness]: allows duplicate courses. Fix is to check if they
// exist and reject or update when key already exists.
template <typename T> void BinarySearchTree<T>::addNode(const T &item) {
  if (Search(item.getKey()).has_value()) {
    std::cout << "Item already exists." << std::endl;
    return;
  }

  // Create a new node with the item and the key (courseID)
  std::unique_ptr<Node<T>> newNode = std::make_unique<Node<T>>(item);

  // If the tree is empty, set the root node
  if (root == nullptr) {
    root = std::move(newNode);
    return;
  }

  Node<T> *current = root.get();
  Node<T> *parent = nullptr;

  // Traverse the tree to find the correct location for the new node
  while (current != nullptr) {
    parent = current;

    // Compare the keys lexicographically (course IDs)
    if (item.getKey() <
        current->getKey()) { // Insert into the left subtree if key issmaller
      current = current->left.get();
    } else { // Insert into the right subtree if key is larger or equal
      current = current->right.get();
    }
  }

  // Insert the new node as the left or right child
  if (item.getKey() < parent->getKey()) {
    parent->left = std::move(newNode);
  } else {
    parent->right = std::move(newNode);
  }
}

/*
 * Note: these traversals require that you have a method for printing your
 * custom item struct. In this implementaion shown I will create a method for
 * printing out the course structs information and pass it into these
 * functions.
 */
// FIXED: (SE)[Defensive]: If the tree is empty, root is null, so node = root
// leaves node null and node->left crashes. This is what causes the print
// before load crash. The fix is to return if node == nullptr.
template <typename T>
void BinarySearchTree<T>::inOrder(Node<T> *node, void (*func)(Node<T> *)) {
  // if node is null.
  if (node == nullptr)
    return;
  // InOrder not left
  if (node->left != nullptr) {
    inOrder(node->left.get(), func);
  }
  func(node);
  if (node->right != nullptr) {
    inOrder(node->right.get(), func);
  }
}

/*
 * post-order traversal.
 */
// FIXED: (SE)[Correctness]: Software design and engineering: Correctness.
// Second function argument is omited. This function is a template,
// so it only compiles when used, and since this was never used, I never caught
// this.
template <typename T>
void BinarySearchTree<T>::postOrder(Node<T> *node, void (*func)(Node<T> *)) {
  // if node is equal to null ptr
  if (node == nullptr)
    return;
  postOrder(node->left.get(), func);
  postOrder(node->right.get(), func);
  func(node);
}

/*
 * pre-order traversal.
 */
// FIXED: (SE)[Correctness]: Software design and engineering: Correctness.
// Second function argument is omited. This function is a template,
// so it only compiles when used, and since this was never used, I never caught
// this.
template <typename T>
void BinarySearchTree<T>::preOrder(Node<T> *node, void (*func)(Node<T> *)) {
  // if node is equal to null ptr
  if (node == nullptr)
    return;
  func(node);
  preOrder(node->left.get(), func);
  preOrder(node->right.get(), func);
}

/**
 * Remove item from Node (Recursive.)
 */
// FIXED: (ALG)[Correctness]: Removing a node with two children copies the
// successor's items but not its key, which causes corruption.
// NOTE: The above bug was fixed by fixing another bug/ineffecncy where the
// nodes key was a copy of courseID thus having the same data stored twice.
// Instead, we now have functions that get key directly from courseID when
// needed, so its not possible for they keys to be mismatched here anymore since
// we no longer need to manage it manually.
template <typename T>
std::unique_ptr<Node<T>>
BinarySearchTree<T>::removeNode(std::unique_ptr<Node<T>> node,
                                const std::string &id) {
  if (node == nullptr)
    return node;

  if (id < node->getKey()) {
    node->left = removeNode(std::move(node->left), id);
  } else if (id > node->getKey()) {
    node->right = removeNode(std::move(node->right), id);
  } else {
    if (node->left == nullptr) {
      return std::move(node->right);
    } else if (node->right == nullptr) {
      return std::move(node->left);
    }
    Node<T> *succ = findMinNode(node->right.get());
    node->item = succ->item;
    node->right = removeNode(std::move(node->right), node->getKey());
  }
  return node;
}

// Helper function to find the minimum value node in a subtree
template <typename T> Node<T> *BinarySearchTree<T>::findMinNode(Node<T> *node) {
  Node<T> *current = node;

  // Find the leftmost node.
  while (current && current->left != nullptr) {
    current = current->left.get();
  }

  return current;
}

/*
 * helper fucntion to insert into BST from rows.
 */
// TODO: (DB)[Data integrity]: Prerequisites are never checked against
// courses, dupe course IDS are accepted. In SQLite A PRIMARY KEY on courseID
// and FOREIGN KEY on Prerequisites enforce both.

#endif
