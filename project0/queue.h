#ifndef PROJECT_0_QUEUE_H_
#define PROJECT_0_QUEUE_H_

// Structs for the linked list
typedef struct node {
    int pid;
    char* cmd;
    struct node *next;
} Node;

typedef struct {
    Node *head;
    int length;
} Queue;

// Add a node to the end of the linked list
int addNode(int pid, char* cmd, Queue *jobs);

// Remove first node with 'pid' = pid from the linked list
int removeNode(int pid, Queue *jobs);

// Returns 1 if linked list is empty, 0 otherwise
int isEmpty(Queue *jobs);

// Prints the linked list as a jobs list in "[index] (pid) Command" format
int printQueue(Queue *jobs);

#endif