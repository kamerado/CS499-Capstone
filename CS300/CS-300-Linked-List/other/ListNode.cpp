#include "ListNode.h"
#include <iostream>
#include <string>

// template <class T> class ListNode {
// public:
//   T data;         // Data field of type T
//   ListNode *next; // Pointer to the next node
//
//   // Default constructor
//   ListNode();
//
//   // Constructor with data
//   ListNode(T idata);
//
//   // Constructor with data and next pointer
//   ListNode(T idata, ListNode<T> *inext);
//
//   // Convert data to string (assuming T has a toString method)
//   std::string toString();
//
//   // Get the next node
//   ListNode<T> *getNext();
//
//   // Set the next pointer
//   void setNext(ListNode<T> *newNext);
//
//   // Get the data
//   T getData();
//
//   // Set the data
//   void setData(T newData);
// };

// Default constructor
template <class T> ListNode<T>::ListNode() {
  T t;            // Default constructor for T (e.g., for Song)
  data = t;       // Initialize data with the default value of T
  next = nullptr; // Set next to nullptr
}

// Constructor with data
template <class T> ListNode<T>::ListNode(T idata) {
  data = idata;   // Assign data
  next = nullptr; // Set next to nullptr
}

// Constructor with data and next pointer
template <class T> ListNode<T>::ListNode(T idata, ListNode<T> *inext) {
  data = idata; // Assign data
  next = inext; // Assign next pointer
}

// Convert data to string
template <class T> std::string ListNode<T>::toString() {
  return data.toString(); // Ensure T has a toString method
}

// Get the next node
template <class T> ListNode<T> *ListNode<T>::getNext() {
  return next; // Return the next pointer
}

// Set the next pointer
template <class T> void ListNode<T>::setNext(ListNode<T> *newNext) {
  next = newNext; // Set next to newNext
}

// Get the data
template <class T> T ListNode<T>::getData() {
  return data; // Return data
}

// Set the data
template <class T> void ListNode<T>::setData(T newData) {
  data = newData; // Set data to newData
}
