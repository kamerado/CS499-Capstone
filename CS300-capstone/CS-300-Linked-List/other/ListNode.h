#ifndef LISTNODE_H
#define LISTNODE_H

#include <string>

template <class T>
class ListNode {
public:
    T data; // Data field of type T
    ListNode* next; // Pointer to the next node

    // Default constructor
    ListNode();

    // Constructor with data
    ListNode(T idata);

    // Constructor with data and next pointer
    ListNode(T idata, ListNode<T>* inext);

    // Convert data to string (assuming T has a toString method)
    std::string toString();

    // Get the next node
    ListNode<T>* getNext();

    // Set the next pointer
    void setNext(ListNode<T>* newNext);

    // Get the data
    T getData();

    // Set the data
    void setData(T newData);
};

// Function prototypes (if needed)
void printList(ListNode<int>* front);
int search(ListNode<int>* front, int value);
void insert(ListNode<int>*& front, int value);
void addNode(ListNode<int>*& front, int value);
void remove(ListNode<int>*& front, int value);

#endif // LISTNODE_H
