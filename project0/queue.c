#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

int addNode(int pid, char* cmd, Queue *jobs)
{
    Node *newEndNode = malloc(sizeof(Node));
    newEndNode->pid = pid;
    newEndNode->next = NULL;
    newEndNode->cmd = cmd;

    if (jobs->head == NULL) {
        jobs->head = newEndNode;
        jobs->length = 1;
    }
    else {
        Node *currentNode;
        currentNode = jobs->head;

        while(currentNode->next != NULL) {
            currentNode = currentNode->next;
        }

        currentNode->next = newEndNode;
        jobs->length++;
    }

    return jobs->length;
}

int removeNode(int pid, Queue *jobs)
{
    if(jobs->head == NULL)
        return -1;
    
    Node *currentNode;
    Node *previousNode;
    currentNode = jobs->head;

    while(currentNode->next != NULL && currentNode->pid != pid) {
        previousNode = currentNode;
        currentNode = currentNode->next;
    }

    if(currentNode->next == NULL)
        return -1;

    previousNode->next = currentNode->next;
    free(currentNode);

    jobs->length--;

    return jobs->length;
}

int isEmpty(Queue *jobs)
{
    if (jobs->length == 0)
        return 1;
    else
        return 0;
}

int printQueue(Queue *jobs)
{
    Node *currentNode = jobs->head;
    int jobPosition = 1;
    while (currentNode != NULL)
    {
        printf("[%d] (%d) %s\n", jobPosition, currentNode->pid, currentNode->cmd);
        currentNode = currentNode->next;
        jobPosition++;
    }

    return jobPosition;
}