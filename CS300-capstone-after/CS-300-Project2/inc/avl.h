#ifndef AVL_H
#define AVL_H

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

// #include "course.h"

/*
 * Struct AVLNode, using typename T for item.
 */
template <typename T> struct AVLNode {
  T item;
  std::unique_ptr<AVLNode> left = nullptr;
  std::unique_ptr<AVLNode> right = nullptr;
  int height = 1;

  // default constructor.
  AVLNode() = default;

  // constructor with item
  AVLNode(const T &aItem) : item(aItem) {}

  const std::string &getKey() const { return item.getKey(); }

  static int heightOf(const std::unique_ptr<AVLNode> &n) {
    return n ? n->height : 0;
  }

  const int balance() const { return heightOf(left) - heightOf(right); }

  void updateHeight() {
    height = 1 + std::max(heightOf(left), heightOf(right));
  }
};

/*
 * Class declaration for AVLTree, using struct AVLNode, and typename T for
 * item type.
 */
// TODO: (ALG)[Effeciency] CourseID are usually sorted, so the BST is basically
// a linked List with O(n) search. Inserting is also O(n), so loading sorted
// data is O(n^2). To fix this, I will replace this with a self balancing AVL
// tree.
template <typename T> class AVLTree {

  // private member variables.
private:
  std::unique_ptr<AVLNode<T>> root;

  // Private member methods.
private:
  std::unique_ptr<AVLNode<T>> rebalance(std::unique_ptr<AVLNode<T>> node);
  std::unique_ptr<AVLNode<T>> rotateRight(std::unique_ptr<AVLNode<T>> y);
  std::unique_ptr<AVLNode<T>> rotateLeft(std::unique_ptr<AVLNode<T>> x);
  std::unique_ptr<AVLNode<T>> addAVLNode(std::unique_ptr<AVLNode<T>> node,
                                         const T &item);
  void inOrder(AVLNode<T> *node,
               const std::function<void(const T &)> &func) const;
  void postOrder(AVLNode<T> *node,
                 const std::function<void(const T &)> &func) const;
  void preOrder(AVLNode<T> *node,
                const std::function<void(const T &)> &func) const;
  std::unique_ptr<AVLNode<T>> removeAVLNode(std::unique_ptr<AVLNode<T>> node,
                                            const std::string &courseID);
  AVLNode<T> *findMinAVLNode(AVLNode<T> *);

  // Constructors
public:
  AVLTree() = default;
  ~AVLTree() { Clear(); }

  // Ownership is unique: no copies (prevents the shared-node double free),
  // but moves are allowed and transfer the whole tree.
  AVLTree(const AVLTree &) = delete;
  AVLTree &operator=(const AVLTree &) = delete;
  AVLTree(AVLTree &&) = default;
  AVLTree &operator=(AVLTree &&) = default;

  // Public member methods.
public:
  void InOrder(const std::function<void(const T &)> &func) const;
  void PostOrder(const std::function<void(const T &)> &func) const;
  void PreOrder(const std::function<void(const T &)> &func) const;
  void Insert(const T &item);
  void Remove(const std::string &itemID);
  std::optional<T> Search(const std::string &itemID) const;
  void Clear();
};

template <typename T> void AVLTree<T>::Clear() {
  std::vector<std::unique_ptr<AVLNode<T>>> stack;
  if (root)
    stack.push_back(std::move(root)); // root is now nullptr

  while (!stack.empty()) {
    std::unique_ptr<AVLNode<T>> node = std::move(stack.back());
    stack.pop_back();
    if (node->left)
      stack.push_back(std::move(node->left));
    if (node->right)
      stack.push_back(std::move(node->right));
    // node goes out of scope here with no children, so no recursion
  }
}

template <typename T>
std::unique_ptr<AVLNode<T>>
AVLTree<T>::rotateRight(std::unique_ptr<AVLNode<T>> y) {
  std::unique_ptr<AVLNode<T>> x = std::move(y->left);
  y->left = std::move(x->right);
  y->updateHeight();
  x->right = std::move(y);
  x->updateHeight();
  return x;
}

template <typename T>
std::unique_ptr<AVLNode<T>>
AVLTree<T>::rotateLeft(std::unique_ptr<AVLNode<T>> x) {
  std::unique_ptr<AVLNode<T>> y = std::move(x->right);
  x->right = std::move(y->left);
  x->updateHeight();
  y->left = std::move(x);
  y->updateHeight();
  return y;
}

template <typename T>
std::unique_ptr<AVLNode<T>>
AVLTree<T>::rebalance(std::unique_ptr<AVLNode<T>> node) {
  int b = node->balance();
  if (b > 1) {
    if (node->left->balance() < 0)
      node->left = rotateLeft(std::move(node->left));
    return rotateRight(std::move(node));
  }
  if (b < -1) {
    if (node->right->balance() > 0)
      node->right = rotateRight(std::move(node->right));
    return rotateLeft(std::move(node));
  }

  return node;
}

/**
 * Traverse the tree in order
 */
// TODO: (ALG)[Effeciency]: clear() is UI logic and it should'nt be in the
// data structure logic. Move this into the print screen function before
// printing.
template <typename T>
void AVLTree<T>::InOrder(const std::function<void(const T &)> &func) const {
  inOrder(root.get(), func);
}

/**
 * Traverse the tree in post-order
 */
template <typename T>
void AVLTree<T>::PostOrder(const std::function<void(const T &)> &func) const {
  postOrder(root.get(), func);
}

/**
 * Traverse the tree in pre-order
 */
template <typename T>
void AVLTree<T>::PreOrder(const std::function<void(const T &)> &func) const {
  preOrder(root.get(), func);
}

/**
 * Insert an node.
 */
template <typename T> void AVLTree<T>::Insert(const T &item) {
  root = addAVLNode(std::move(root), item);
}

/**
 * Remove a node.
 */
template <typename T> void AVLTree<T>::Remove(const std::string &itemID) {
  root = removeAVLNode(std::move(root), itemID);
}

/**
 * Search for a node.
 */
template <typename T>
std::optional<T> AVLTree<T>::Search(const std::string &itemID) const {
  // set current node equal to root
  AVLNode<T> *curr = root.get();

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
template <typename T>
std::unique_ptr<AVLNode<T>>
AVLTree<T>::addAVLNode(std::unique_ptr<AVLNode<T>> node, const T &item) {
  // If the tree is empty, set the root node
  if (node == nullptr) {
    return std::make_unique<AVLNode<T>>(item);
  }

  if (std::string(item.getKey()) < std::string(node->getKey()))
    node->left = addAVLNode(std::move(node->left), item);
  else if (std::string(item.getKey()) > std::string(node->getKey()))
    node->right = addAVLNode(std::move(node->right), item);
  else {
    std::cout << "Error: Item already exists, aborting." << std::endl;
    return node;
  }

  node->updateHeight();

  return rebalance(std::move(node));
}

/*
 * Note: these traversals require that you have a method for printing your
 * custom item struct. In this implementaion shown I will create a method for
 * printing out the course structs information and pass it into these
 * functions.
 */
template <typename T>
void AVLTree<T>::inOrder(AVLNode<T> *node,
                         const std::function<void(const T &)> &func) const {
  // if node is null.
  if (node == nullptr)
    return;
  // InOrder not left
  if (node->left != nullptr) {
    inOrder(node->left.get(), func);
  }
  func(node->item);
  if (node->right != nullptr) {
    inOrder(node->right.get(), func);
  }
}

/*
 * post-order traversal.
 */
template <typename T>
void AVLTree<T>::postOrder(AVLNode<T> *node,
                           const std::function<void(const T &)> &func) const {
  // if node is equal to null ptr
  if (node == nullptr)
    return;
  postOrder(node->left.get(), func);
  postOrder(node->right.get(), func);
  func(node->item);
}

/*
 * pre-order traversal.
 */
template <typename T>
void AVLTree<T>::preOrder(AVLNode<T> *node,
                          const std::function<void(const T &)> &func) const {
  // if node is equal to null ptr
  if (node == nullptr)
    return;
  func(node->item);
  preOrder(node->left.get(), func);
  preOrder(node->right.get(), func);
}

/**
 * Remove item from AVLNode (Recursive.)
 */
// NOTE: The above bug was fixed by fixing another bug/ineffecncy where the
// nodes key was a copy of courseID thus having the same data stored twice.
// Instead, we now have functions that get key directly from courseID when
// needed, so its not possible for they keys to be mismatched here anymore since
// we no longer need to manage it manually.
template <typename T>
std::unique_ptr<AVLNode<T>>
AVLTree<T>::removeAVLNode(std::unique_ptr<AVLNode<T>> node,
                          const std::string &id) {
  if (node == nullptr)
    return node;

  if (id < node->getKey()) {
    node->left = removeAVLNode(std::move(node->left), id);
  } else if (id > node->getKey()) {
    node->right = removeAVLNode(std::move(node->right), id);
  } else {
    if (node->left == nullptr) {
      return std::move(node->right);
    } else if (node->right == nullptr) {
      return std::move(node->left);
    }
    AVLNode<T> *succ = findMinAVLNode(node->right.get());
    node->item = succ->item;
    node->right = removeAVLNode(std::move(node->right), node->getKey());
  }

  node->updateHeight();

  return rebalance(std::move(node));
}

// Helper function to find the minimum value node in a subtree
template <typename T> AVLNode<T> *AVLTree<T>::findMinAVLNode(AVLNode<T> *node) {
  AVLNode<T> *current = node;

  // Find the leftmost node.
  while (current && current->left != nullptr) {
    current = current->left.get();
  }

  return current;
}

#endif
