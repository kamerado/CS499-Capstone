#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#define DEFAULT_SIZE 179

typedef std::vector<std::string> Row;
typedef std::vector<Row> Rows;

void clear() {
  // CSI[2J clears screen, CSI[H moves the cursor to top-left corner
  std::cout << "\x1B[2J\x1B[H";
}

void getInput(std::string &key) {
  std::cin >> key;
  std::cin.clear();
  std::cout << std::endl;
}

/*
 * helper fucntion to egt input from user for main menu.
 */
bool getInput(int &input) {
  std::cout << "Please select an option: ";
  std::string strCinContent;
  std::cin >> strCinContent;
  try {
    input = stoi(strCinContent, 0, 10);
  } catch (std::invalid_argument err) {
    clear();
    std::cout << "You can only input integer values." << std::endl;
    return false;
  }
  std::cin.clear();
  std::cout << std::endl;
  return true;
}

// Struct course.
struct Course {
  std::string courseID;
  std::string courseTitle;
  std::vector<std::string> prereq;

  // default constructor.
  Course() {
    courseID = "\0";
    courseTitle = "\0";
  }

  // constructor with id.
  Course(std::string id) : Course() { courseID = id; }

  // cosntructor with id, title.
  Course(std::string id, std::string title) : Course(id) {
    courseTitle = title;
  }

  // constructor with id, title, prereqs.
  Course(std::string id, std::string title, std::vector<std::string> preReq)
      : Course(id, title) {
    if (preReq.size() > 0) {
      prereq = preReq;
    }
  }

  // Func to print course data.
  void print() const {
    std::cout << "Course ID: " << courseID << "\n"
              << "Course Title: " << courseTitle << "\n";
    if (prereq.size() > 0) {
      auto it = prereq.begin();
      std::cout << "Prerequisites: " << "\n";
      for (; it < prereq.end(); it++) {
        std::cout << "    " << *it << "\n";
      }
      std::cout << std::endl;
    }
  }
};

/*
 * Struct Node, using typename T for item.
 */
template <typename T> struct Node {
  T item;
  std::string key;
  Node *left;
  Node *right;

  // default constructor.
  Node() {
    left = nullptr;
    right = nullptr;
  }

  // constructor with item
  Node(T aItem) : Node() { item = aItem; }

  // constructor with item, key
  Node(T aItem, std::string aKey) : Node(aItem) { key = aKey; }
};

/*
 * Class declaration for BinarySearchTree, using struct Node, and typename T for
 * item type.
 */
// TODO: CourseID are usually sorted, so the BST is basically a linked List with
// O(n) search. To fix this, I will replace this with a self balancing AVL tree.
template <typename T> class BinarySearchTree {

  // private member variables.
private:
  Node<T> *root;

  // Private member methods.
private:
  void addNode(T item, std::string &id);
  void inOrder(Node<T> *node, void (*func)(Node<T> *));
  void postOrder(Node<T> *node, void (*func)(Node<T> *));
  void preOrder(Node<T> *node, void (*func)(Node<T> *));
  Node<T> *removeNode(Node<T> *node, std::string courseID);
  void DeleteTree(Node<T> *);
  Node<T> *findMinNode(Node<T> *);

  // Constructors
public:
  BinarySearchTree();
  virtual ~BinarySearchTree();

  // Public member methods.
public:
  void InOrder(void (*func)(Node<T> *));
  void PostOrder(void (*func)(Node<T> *));
  void PreOrder(void (*func)(Node<T> *));
  void Insert(T item, std::string &id);
  void Remove(std::string itemID);
  T Search(std::string itemID);
};

/*
 * Default constructor.
 */
template <typename T> BinarySearchTree<T>::BinarySearchTree() {
  root = nullptr;
}

/*
 * helper function to delete nodes recursivly for deconstructor.
 */
template <typename T> void BinarySearchTree<T>::DeleteTree(Node<T> *curr) {
  if (curr == nullptr)
    return;

  if (curr->left != nullptr) {
    DeleteTree(curr->left);
  }

  if (curr->right != nullptr) {
    DeleteTree(curr->right);
  }

  delete curr;
  return;
}

/*
 * deconstructor.
 */
template <typename T> BinarySearchTree<T>::~BinarySearchTree() {
  DeleteTree(root);
}

/**
 * Traverse the tree in order
 */
template <typename T>
void BinarySearchTree<T>::InOrder(void (*func)(Node<T> *)) {
  clear();
  inOrder(root, func);
}

/**
 * Traverse the tree in post-order
 */
template <typename T>
void BinarySearchTree<T>::PostOrder(void (*func)(Node<T> *)) {
  postOrder(root, func);
}

/**
 * Traverse the tree in pre-order
 */
template <typename T>
void BinarySearchTree<T>::PreOrder(void (*func)(Node<T> *)) {
  preOrder(root, func);
}

/**
 * Insert a bid
 */
template <typename T>
void BinarySearchTree<T>::Insert(T item, std::string &id) {
  addNode(item, id);
}

/**
 * Remove a bid
 */
// TODO: Software design and Engineering
// get rid of dangling pointer from returned values.
// The function removeNode returns a pointer that is never used.
template <typename T> void BinarySearchTree<T>::Remove(std::string itemID) {
  // remove node root bidID
  removeNode(root, itemID);
}

/**
 * Search for a bid
 */
template <typename T> T BinarySearchTree<T>::Search(std::string itemID) {
  // set current node equal to root
  Node<T> *curr = root;

  // keep looping downwards until bottom reached or matching bidId found
  while (curr != nullptr) {
    // if match found, return current bid
    if (itemID == curr->key) {
      return curr->item;
      // if bid is smaller than current node then traverse left
    } else if (itemID < curr->key) {
      curr = curr->left;

      // else larger so traverse right
    } else {
      curr = curr->right;
    }
  }

  T course;
  return course;
}

/*
 * Function to add node to bst.
 */
template <typename T>
void BinarySearchTree<T>::addNode(T item, std::string &key) {
  // Create a new node with the item and the key (courseID)
  Node<T> *newNode = new Node<T>(item);
  newNode->key = key;

  // If the tree is empty, set the root node
  if (root == nullptr) {
    root = newNode;
    return;
  }

  Node<T> *current = root;
  Node<T> *parent = nullptr;

  // Traverse the tree to find the correct location for the new node
  while (current != nullptr) {
    parent = current;

    // Compare the keys lexicographically (course IDs)
    if (key < current->key) { // Insert into the left subtree if key is smaller
      current = current->left;
    } else { // Insert into the right subtree if key is larger or equal
      current = current->right;
    }
  }

  // Insert the new node as the left or right child
  if (key < parent->key) {
    parent->left = newNode;
  } else {
    parent->right = newNode;
  }
}

/*
 * Note: these traversals require that you have a method for printing your
 * custom item struct. In this implementaion shown I will create a method for
 * printing out the course structs information and pass it into these
 * functions. This function is on line 516.
 */
template <typename T>
void BinarySearchTree<T>::inOrder(Node<T> *node, void (*func)(Node<T> *)) {
  // if node is not equal to null ptr
  if (node == nullptr)
    node = root;
  // InOrder not left
  if (node->left != nullptr) {
    inOrder(node->left, func);
  }
  func(node);
  if (node->right != nullptr) {
    inOrder(node->right, func);
  }
}

/*
 * post-order traversal.
 */
// TODO: Software design and engineering: Correctness.
// Second function argument is omited. This function is unused in this
// implementation, So it compiles. if it was ever used, then it would fail.
template <typename T>
void BinarySearchTree<T>::postOrder(Node<T> *node, void (*func)(Node<T> *)) {
  // if node is not equal to null ptr
  if (node == nullptr)
    return;
  postOrder(node->left);
  postOrder(node->right);
  func(node);
}

/*
 * pre-order traversal.
 */
// TODO: Software design and engineering: Correctness.
// Second function argument is omited. This function is unused in this
// implementation, So it compiles. if it was ever used, then it would fail.
template <typename T>
void BinarySearchTree<T>::preOrder(Node<T> *node, void (*func)(Node<T> *)) {
  // if node is not equal to null ptr
  if (node == nullptr)
    return;
  func(node);
  preOrder(node->left);
  preOrder(node->right);
}

/**
 * Remove item from Node (Recursive.)
 */
// TODO: Software engineering: Correctness
// Removing a node with two children copies the successor's items
// but not its key, which causes corruption.
template <typename T>
Node<T> *BinarySearchTree<T>::removeNode(Node<T> *node, std::string id) {
  if (node == nullptr)
    return node;

  if (id < node->key) {
    node->left = removeNode(node->left, id);
  } else if (id > node->key) {
    node->right = removeNode(node->right, id);
  } else {
    if (node->left == nullptr && node->right == nullptr) {
      delete node;
      return nullptr;
    } else if (node->left == nullptr) {
      Node<T> *tmp = node->right;
      delete node;
      return tmp;
    } else if (node->right == nullptr) {
      Node<T> *tmp = node->left;
      delete node;
      return tmp;
    } else {
      Node<T> *tmp = findMinNode(node->right);

      node->item = tmp->item;
      // TODO: Nodes key still is the old CourseID/key.

      node->right = removeNode(node->right, tmp->key);
    }
  }
  return node; // TODO: consider deleting this node here.
}

// Helper function to find the minimum value node in a subtree
template <typename T> Node<T> *BinarySearchTree<T>::findMinNode(Node<T> *node) {
  Node<T> *current = node;

  // Find the leftmost leaf
  while (current && current->left != nullptr) {
    current = current->left;
  }

  return current;
}
/*
 * helper function to read file into vector.
 */
void readFile(std::string csvPath, std::vector<std::string> &_originalFile) {
  std::string line;

  std::ifstream ifile(csvPath.c_str());
  if (ifile.is_open()) {
    while (
        ifile
            .good()) { // Gets line and adds it to vector<string> _originalFile.
      std::getline(ifile, line);
      if (line != "") {
        _originalFile.push_back(line);
      }
    }
    ifile.close();

    if (_originalFile.size() == 0) { // If no data, exit program.
      std::cout << "error: no data in csv file." << std::endl;
      exit(1);
    }
  }
}

/*
 * helper function to read from vector into rows with individual columns.
 */
// TODO: Software Design and Engineering: Memory
// Allocating new row() each loop but never frees them.
Rows *FileToRows(std::vector<std::string> &_originalFile) {
  Rows *rows = new Rows();

  // Iterate through each line in file contents.
  std::vector<std::string>::iterator it;
  it = _originalFile.begin();

  for (; it != _originalFile.end(); it++) { // For every line.
    int tokenStart = 0;
    unsigned int i = 0;

    Row *row = new Row();
    for (; i < it->length();
         i++) { // Parse every char, if ',' then push substr into Row.
      if (it->at(i) == ',') {
        row->push_back(it->substr(tokenStart, i - tokenStart));
        tokenStart = i + 1; // Update start pos.
      }
    }

    row->push_back(it->substr(tokenStart,
                              it->length() - tokenStart)); // Push last section.

    if (row->size() < 2) { // if Row is less than two params error.
      std::cout << "Error: CSV is formated incorrectly." << std::endl;
      ;
      exit(1);
    }
    rows->push_back(*row); // push row into Rows.
    row->clear();          // Reset Row.
  }

  return rows; // return rows.
}

/*
 * helper fucntion to insert into BST from rows.
 */
void RowsToBST(BinarySearchTree<Course> &bst, Rows *rows) {

  /*
   * iterate over rows.
   */
  Rows::iterator it2 = rows->begin();

  int n = 0;
  for (; it2 != rows->end(); it2++) { // for each row.

    // Get data.
    std::string courseid = it2->at(0);
    std::string coursetitle = it2->at(1);
    std::vector<std::string> prereqs;
    for (int i = 2; i < it2->size(); i++) {
      prereqs.push_back(it2->at(i));
    }

    // add data to Course.
    Course course(courseid, coursetitle, prereqs);
    bst.Insert(course, courseid); // add Course to BST<T> where T = Course.
  }
}

/*
 * function to parse csv file.
 */
// TODO: Software Design and Engineering: Memory
// Input handling does not account for certain fields such as quaoted feilds,
// Line endings, stoi out of range errors, and abnormal inputs.
// We should add better input validation here.
// TODO: DataBases: Add sql implementation with ability to add rows via csv.
// TODO: DataBases: Here, if we load the same file twice, there are no checks to
// prevent duplicates (SQL should prevent this automatically but woth
// mentioning).
void csvParse(BinarySearchTree<Course> &bst, std::string &csvpath,
              std::vector<std::string> &_originalFile) {
  // read csv file into vector of lines.
  readFile(csvpath, _originalFile);

  // Read contents to rows.
  Rows *rows = FileToRows(_originalFile);

  // insert rows into bst.
  RowsToBST(bst, rows);
}

/*
 * helper function to get user input and search for input in BST.
 */
void search(BinarySearchTree<Course> &bst) {
  clear();

  std::string key;
  std::cout << "Enter courseID: ";
  getInput(key);

  std::transform(key.begin(), key.end(), key.begin(),
                 [](unsigned char c) { return std::toupper(c); });

  clear();

  Course tmp = bst.Search(key);
  if (tmp.courseID == key) {
    tmp.print();
  } else {
    std::cout << "No valid search results." << std::endl;
  }
}

/*
 * helper funciton to check if file exists.
 */
bool exists(const std::string &name) { return std::filesystem::exists(name); }
/*
 * function to print out the custom item type.
 */
void print(Node<Course> *node) { node->item.print(); }

/*
 * helper fucntion to get file name as input from user.
 */
void getPath(std::string &csvpath) {
  std::cout << "\nPlease enter csv file name: " << std::endl;
  getInput(csvpath);

  while (!exists(csvpath)) {
    clear();
    std::cout << "\nFilename invalid." << std::endl;

    std::cout << "\nPlease enter csv file name: " << std::endl;
    getInput(csvpath);
  }
}

/*
 * helper function to print menu.
 */
void printMenu() {
  std::cout << "\n\n    1. Load File Into BST.\n";
  std::cout << "    2. Print Course List.\n";
  std::cout << "    3. Print Course.\n";
  std::cout << "    9. Exit.\n\n";
}

/*
 * helper fucntion to decide program logic based on user input.
 */
// TODO: Software engineering: Defencive programming
void handleInput(BinarySearchTree<Course> &bst, int &input, bool &loop,
                 std::string csvpath, std::vector<std::string> _originalFile) {
  switch (input) {
  case 1:
    csvParse(bst, csvpath, _originalFile);
    clear();
    break;
  case 2:
    // TODO: Make a check so that you cant call print
    // before loading CSV file into BST.
    clear();
    bst.InOrder(print);
    break;
  case 3:
    search(bst);
    break;
  case 9:
    loop = false;
    break;
  default:
    clear();
    std::cout << "Please input a valid option." << std::endl;
    break;
  }
}

/*
 * main fucntion.
 */
int main() {
  /*
   * Declare variables for functon implementaions.
   */
  BinarySearchTree<Course> bst;
  std::vector<std::string> _originalFile;
  std::vector<std::string> _header;
  std::string csvpath;
  bool loop = true;
  int input;

  getPath(csvpath);
  clear();

  std::cout << "\nWelcome to the course planner." << std::endl;
  while (loop) {
    printMenu();
    if (getInput(input)) {
      handleInput(bst, input, loop, csvpath, _originalFile);
    }
  }

  clear();
  std::cout << "Goodbye." << std::endl;
}
