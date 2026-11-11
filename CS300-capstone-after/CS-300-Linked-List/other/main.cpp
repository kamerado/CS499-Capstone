#include "ListNode.h" // Ensure this only contains class declarations and function prototypes
#include <iostream>

// Function prototypes
int search(ListNode<int> *front, int value);
void insert(ListNode<int> *&front, int value);
void addNode(ListNode<int> *&front, int value);
void remove(ListNode<int> *&front, int value);
void printList(ListNode<int> *front);

class LinkedList {
public:
  ListNode<int> *head;
  ListNode<int> *tail;
  int size;

public:
  LinkedList();
  virtual ~LinkedList();
  void Append(ListNode<int> node);
  void Prepend(ListNode<int> node);
  void PrintList();
  void Remove(int value);
  int Search(int value);
  int Size();
};

/**
 * Default constructor
 */
LinkedList::LinkedList() {
  // FIXME (1): Initialize housekeeping variables
  // set head and tail equal to nullptr
  head = nullptr;
  tail = nullptr;
}

/**
 * Destructor
 */
LinkedList::~LinkedList() {
  // start at the head
  ListNode<int> *current = head;
  ListNode<int> *temp;

  // loop over each node, detach from list then delete
  while (current != nullptr) {
    temp = current;          // hang on to current node
    current = current->next; // make current the next node
    delete temp;             // delete the orphan node
  }
}

/**
 * Append a new bid to the end of the list
 */
void LinkedList::Append(ListNode<int> node) {
  // FIXME (2): Implement append logic
  // Create new node
  ListNode<int> *newNode = &node;
  // if there is nothing at the head...
  if (head == nullptr) {
    // new node becomes the head and the tail
    head = newNode;
    tail = newNode;
  } else { // else
    //  make current tail node point to the new node
    tail->next = newNode;
    //  and tail becomes the new node
    tail = newNode;
  }
  // increase size count
  size++;
}

/**
 * Prepend a new bid to the start of the list
 */
void LinkedList::Prepend(ListNode<int> node) {
  // FIXME (3): Implement prepend logic
  // Create new node
  ListNode<int> *newNode = &node;
  // if there is already something at the head...
  if (head != nullptr) {
    // new node points to current head as its next node
    newNode->next = head;
    // head now becomes the new node
    head = newNode;
  }
  // increase size count
  size++;
}

/**
 * Simple output of all bids in the list
 */
void LinkedList::PrintList() {
  // FIXME (4): Implement print logic
  // start at the head
  auto it = head;
  // while loop over each node looking for a match
  while (it != nullptr) {
    // output current bidID, title, amount and fund
    std::cout << it->data << it->data << it->data << it->data << std::endl;
    // set current equal to next
    it = it->next;
  }
}

/**
 * Remove a specified bid
 *
 * @param bidId The bid id to remove from the list
 */
void LinkedList::Remove(int value) {
  // FIXME (5): Implement remove logic
  // special case if matching node is the head
  if (head->data == value) {
    head = head->next;
    size--;
    return;
  }
  // make head point to the next node in the list
  // decrease size count
  // return

  auto it = head;
  while (it->next != nullptr) {
    if (it->next->data == value) {
      auto delNode = it->next;
      it->next = delNode->next;
      delete delNode;
      size--;
      return;
    }

    it = it->next;
  }

  return;
}

/**
 * Search for the specified bidId
 *
 * @param bidId The bid id to search for
 */
int LinkedList::Search(int value) {
  // FIXME (6): Implement search logic
  // special case if matching bid is the head
  if (head->data == value) {
    return head->data;
  } else if (tail->data == value) {
    return tail->data;
  } else {
    auto it = head->next;
    while (it != tail) {
      if (it->data == value) {
        return it->data;
      }

      it = it->next;
    }

    return -1;
  }
}

int main() {
  ListNode<int> *front = nullptr;
  printList(front);
  std::cout << "**********************\n";

  addNode(front, 1);
  printList(front);
  std::cout << "**********************\n";

  addNode(front, 2);
  printList(front);
  std::cout << "**********************\n";

  addNode(front, 4);
  printList(front);
  std::cout << "**********************\n";

  insert(front, 3);
  printList(front);
  std::cout << "**********************\n";

  remove(front, 1);
  printList(front);
  std::cout << "**********************\n";

  insert(front, 1);
  printList(front);
  std::cout << "**********************\n";

  remove(front, 3);
  printList(front);
  std::cout << "**********************\n";

  remove(front, 4);
  printList(front);
  std::cout << "**********************\n";

  remove(front, 5);
  printList(front);
  std::cout << "**********************\n";

  remove(front, 2);
  printList(front);
  std::cout << "**********************\n";

  remove(front, 1);
  printList(front);
  std::cout << "**********************\n";

  insert(front, 4);
  printList(front);
  std::cout << "**********************\n";

  insert(front, 1);
  printList(front);
  std::cout << "**********************\n";

  return 0;
}

void printList(ListNode<int> *front) {
  ListNode<int> *current = front;
  while (current != nullptr) {
    std::cout << current->getData() << "\n";
    current = current->getNext();
  }
  if (front == nullptr) {
    std::cout << "EMPTY LIST \n";
  }
}

// Implement other functions (search, insert, addNode, remove) here...
