//============================================================================
// Name        : HashTable.cpp
// Author      : for my eyes only
// Version     : 1.0
// Copyright   : Copyright © 2023 SNHU COCE
// Description : Lab 4-2 Hash Table
//============================================================================

#include <algorithm>
#include <climits>
#include <cstddef>
#include <iostream>
#include <string> // atoi
#include <time.h>

#include "CSVparser.hpp"

using namespace std;

//============================================================================
// Global definitions visible to all methods and classes
//============================================================================

const unsigned int DEFAULT_SIZE = 20000;

// forward declarations
double strToDouble(string str, char ch);

// define a structure to hold bid information
struct Bid {
  string bidId; // unique identifier
  string title;
  string fund;
  double amount;
  Bid() { amount = 0.0; }
};

//============================================================================
// Hash Table class definition
//============================================================================

/**
 * Define a class containing data members and methods to
 * implement a hash table with chaining.
 */
class HashTable {

private:
  // Define structures to hold bids
  struct Node {
    Bid bid;
    unsigned int key;
    Node *next;

    // default constructor
    Node() {
      key = UINT_MAX;
      next = nullptr;
    }

    // initialize with a bid
    Node(Bid aBid) : Node() { bid = aBid; }

    // initialize with a bid and a key
    Node(Bid aBid, unsigned int aKey) {
      bid = aBid;
      key = aKey;
      next = nullptr;
    }
  };

  vector<Node> nodes;

  unsigned int tableSize = DEFAULT_SIZE;

  unsigned int hash(int key);

  int findLow();
  int findMAx();

public:
  HashTable();
  HashTable(unsigned int size);
  virtual ~HashTable();
  void Insert(Bid bid);
  void PrintAll();
  void Remove(string bidId);
  Bid Search(string bidId);
  size_t Size();
};

/**
 * Default constructor
 */
HashTable::HashTable() {

  // Initalize node structure by resizing tableSize
  nodes.resize(tableSize, Node());
}

/**
 * Constructor for specifying size of the table
 * Use to improve efficiency of hashing algorithm
 * by reducing collisions without wasting memory.
 */
HashTable::HashTable(unsigned int size) {
  // invoke local tableSize to size with this->
  this->tableSize = size;
  // resize nodes size
  nodes.resize(this->tableSize);
}

/**
 * Destructor
 */
HashTable::~HashTable() {
  Node *curr, *tmp;

  for (int i = 0; i < this->tableSize; i++) {
    curr = &this->nodes.at(i);
    while (curr->key != UINT_MAX) {
      tmp = curr;
      curr = curr->next;
      delete tmp;
    }
  }

  this->nodes.clear();
  // erase nodes beginning
}

/**
 * Calculates the hash value of a given key.
 * Note that key is specifically defined as
 * unsigned int to prevent undefined results
 * of a negative list index.
 *
 * @param key The key to hash
 * @return The calculated hash
 */
unsigned int HashTable::hash(int key) {

  // return key mod tableSize
  return (key - 79000) % tableSize;
}

/**
 * Insert a bid
 *
 * @param bid The bid to insert
 */
void HashTable::Insert(Bid bid) {
  // FIXME (4): Implement logic to insert a bid
  // create the key for the given bid
  unsigned int key = hash(stoi(bid.bidId));
  Node *prev = nullptr;
  Node *curr = &nodes.at(key);

  while (curr != nullptr && curr->key != UINT_MAX) {
    prev = curr;
    curr = curr->next;
  }

  // if no entry found for the key
  if (curr == &nodes.at(key)) {
    // assign this node to the key position
    curr->bid = bid;
    curr->key = key;
  } else if (curr == nullptr) { // else if node is not used
    curr = new Node(bid, key);
    if (prev != nullptr) {
      prev->next = curr;
    }
  } else {
    curr->bid = bid;
    curr->key = key;
  }
}

/**
 * Print all bids
 */
void HashTable::PrintAll() {
  for (unsigned int i = 0; i < tableSize; ++i) {
    Node *curr = &nodes.at(i);
    while (curr != nullptr && curr->key != UINT_MAX) {
      cout << "Key " << curr->key << ": " << curr->bid.bidId << " | "
           << curr->bid.title << " | " << curr->bid.amount << " | "
           << curr->bid.fund << endl;
      curr = curr->next;
    }
  }
}

/**
 * Remove a bid
 *
 * @param bidId The bid id to search for
 */
void HashTable::Remove(string bidId) {
  // Calculate the key
  unsigned int key = hash(stoi(bidId));
  Node *curr = &nodes.at(key);
  Node *prev = nullptr;

  // Search for the node to remove
  while (curr != nullptr && curr->bid.bidId != bidId) {
    prev = curr;
    curr = curr->next;
  }

  // If bid is not found, return
  if (curr == nullptr)
    return;

  // If the node to be deleted is the head node
  if (curr == &nodes.at(key)) {
    if (curr->next != nullptr) {
      nodes.at(key) = *(curr->next); // Replace head with the next node
    } else {
      curr->key = UINT_MAX; // Mark as empty
      curr->bid = Bid();
    }
  } else { // For other nodes, bypass the node to remove it
    prev->next = curr->next;
    delete curr;
  }
}

/**
 * Search for the specified bidId
 *
 * @param bidId The bid id to search for
 */
Bid HashTable::Search(string bidId) {
  Bid bid;
  // Create the key for the given bid ID
  unsigned int key = hash(stoi(bidId));
  Node *curr = &nodes.at(key);

  // Traverse the chain to find the matching bidId
  while (curr != nullptr) {
    if (curr->key != UINT_MAX && curr->bid.bidId == bidId) {
      return curr->bid; // Return the matching bid
    }
    curr = curr->next;
  }
  return bid;
}

//============================================================================
// Static methods used for testing
//============================================================================

/**
 * Display the bid information to the console (std::out)
 *
 * @param bid struct containing the bid info
 */
void displayBid(Bid bid) {
  cout << bid.bidId << ": " << bid.title << " | " << bid.amount << " | "
       << bid.fund << endl;
  return;
}

/**
 * Load a CSV file containing bids into a container
 *
 * @param csvPath the path to the CSV file to load
 * @return a container holding all the bids read
 */
void loadBids(string csvPath, HashTable *hashTable) {
  cout << "Loading CSV file " << csvPath << endl;

  // initialize the CSV Parser using the given path
  csv::Parser file = csv::Parser(csvPath);

  // read and display header row - optional
  vector<string> header = file.getHeader();
  for (auto const &c : header) {
    cout << c << " | ";
  }
  cout << "" << endl;

  try {
    // loop to read rows of a CSV file
    for (unsigned int i = 0; i < file.rowCount(); i++) {

      // Create a data structure and add to the collection of bids
      Bid bid;
      bid.bidId = file[i][1];
      bid.title = file[i][0];
      bid.fund = file[i][8];
      bid.amount = strToDouble(file[i][4], '$');

      // cout << "Item: " << bid.title << ", Fund: " << bid.fund << ", Amount:
      // "
      // << bid.amount << endl;

      // push this bid to the end
      hashTable->Insert(bid);
    }
  } catch (csv::Error &e) {
    std::cerr << e.what() << std::endl;
  }
}

/**
 * Simple C function to convert a string to a double
 * after stripping out unwanted char
 *
 * credit: http://stackoverflow.com/a/24875936
 *
 * @param ch The character to strip out
 */
double strToDouble(string str, char ch) {
  str.erase(remove(str.begin(), str.end(), ch), str.end());
  return atof(str.c_str());
}

/**
 * The one and only main() method
 */
int main(int argc, char *argv[]) {

  // process command line arguments
  string csvPath, bidKey;
  switch (argc) {
  case 2:
    csvPath = argv[1];
    bidKey = "98223";
    break;
  case 3:
    csvPath = argv[1];
    bidKey = argv[2];
    break;
  default:
    csvPath = "eBid_Monthly_Sales.csv";
    bidKey = "98223";
  }

  // Define a timer variable
  clock_t ticks;

  // Define a hash table to hold all the bids
  HashTable *bidTable;

  Bid bid;
  bidTable = new HashTable();

  int choice = 0;
  while (choice != 9) {
    cout << "Menu:" << endl;
    cout << "  1. Load Bids" << endl;
    cout << "  2. Display All Bids" << endl;
    cout << "  3. Find Bid" << endl;
    cout << "  4. Remove Bid" << endl;
    cout << "  9. Exit" << endl;
    cout << "Enter choice: ";
    cin >> choice;

    switch (choice) {

    case 1:

      // Initialize a timer variable before loading bids
      ticks = clock();

      // Complete the method call to load the bids
      loadBids(csvPath, bidTable);

      // Calculate elapsed time and display result
      ticks = clock() - ticks; // current clock ticks minus starting clock ticks
      cout << "time: " << ticks << " clock ticks" << endl;
      cout << "time: " << ticks * 1.0 / CLOCKS_PER_SEC << " seconds" << endl;
      break;

    case 2:
      bidTable->PrintAll();
      break;

    case 3:
      ticks = clock();

      bid = bidTable->Search(bidKey);

      ticks = clock() - ticks; // current clock ticks minus starting clock ticks

      if (!bid.bidId.empty()) {
        displayBid(bid);
      } else {
        cout << "Bid Id " << bidKey << " not found." << endl;
      }

      cout << "time: " << ticks << " clock ticks" << endl;
      cout << "time: " << ticks * 1.0 / CLOCKS_PER_SEC << " seconds" << endl;
      break;

    case 4:
      bidTable->Remove(bidKey);
      break;
    }
  }

  cout << "Good bye." << endl;

  return 0;
}
