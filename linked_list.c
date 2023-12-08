#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <pthread.h>
# include "execute_command.h"
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/mman.h>
#include<semaphore.h>
#include <assert.h>

// define a struct for a unit in the single linked list
typedef struct node 
{
    pthread_t thread_id; // client's thread id
    int semaphore_id; // semaphore id
    char input[1024]; // the message the client input
    int remaining_time; 
    int round; // the round of the client
    // The shell command's time will be set to -1
    // The program's time will decrement by 1 every second
    int runned_time; // the runned time of the client
    int total_time;
    int total_progress;
    struct node *next;
    int socket;
    int client_id;
} node;

// create a node which contains the execution's information
node* create_node(pthread_t thread_id, int semaphore_id, char input[1024], int remaining_time, int round,int runned_time, int socket)
{
    node* new_node = (node*)malloc(sizeof(node));
    memset(new_node, 0, sizeof(node));
    new_node->thread_id = thread_id;
    new_node->semaphore_id = semaphore_id;
    strcpy(new_node->input, input);
    new_node->remaining_time = remaining_time;
    new_node->next = NULL;
    new_node->round = round;
    new_node->runned_time = runned_time;
    new_node->total_time = remaining_time;
    new_node->total_progress =0;
    new_node->socket = socket;
    new_node->client_id = semaphore_id;
    return new_node;
}

// a function to insert a node at the end of the linked list
void tail_insert(node **head, node *new_node) 
{
    if (new_node == NULL) 
    {
        printf("Error: new_node is NULL\n");
        return; // new_node must be allocated before calling this function
    }

    if (*head == NULL) 
    {
        *head = new_node;
        return;
    }

    node *last = *head;
    while (last->next != NULL) 
    {
        last = last->next;// Traverse the linked list until the last node
    }

    last->next = new_node;
    new_node->next = NULL; // Ensure the new node's next is NULL
}

// a function to insert a node at the beginning of the linked list
void head_insert(node **head, node *new_node) 
{
    if (new_node == NULL) 
    {
        printf("Error: new_node is NULL\n");
        return; // new_node must be allocated before calling this function
    }

    if (*head == NULL) 
    {
        *head = new_node;
        return;
    }

    new_node->next = *head;
    *head = new_node;
}

// a function to delete a node from the linked list
void delete_node(node **head, pthread_t thread_id) 
{
    if (*head == NULL) 
    {
        return;
    }

    node *current = *head;
    node *previous = NULL;
    while (current != NULL) 
    {
        if (current->thread_id == thread_id) 
        {
            if (previous == NULL) 
            {
                *head = current->next;
            }
            else 
            {
                previous->next = current->next;
            }
            free(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}

// a function to search for a node in the linked list
node *search_node(node *head, pthread_t thread_id) 
{
    node *current = head;
    while (current != NULL) 
    {
        if (current->thread_id == thread_id) 
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// a function to print a node
void print_node(const node *n) 
{
    //skip header
    n = n->next;
    if (n != NULL) 
    {
        printf("Thread ID: %p, Semaphore ID: %d, Input: %s, Remaining Time: %d\n, Round: %d, Runned Time: %d\n", 
                n->thread_id, n->semaphore_id, n->input, n->remaining_time,n->round,n->runned_time);
               
    }
    else 
    {
        printf("NULL\n");
    }
}
// a function to print the linked list
void print_list(const node *head) 
{
    const node *current = head;
    while (current != NULL) 
    {
        print_node(current);
        current = current->next;
    }
}
// a function to get the length of the linked list
int get_list_length(const node *head) 
{
    int length = 0;
    const node *current = head;
    while (current != NULL) 
    {
        length++;
        current = current->next;
    }
    return length;
}


// Test functions
/*

void test_create_node() {
    pthread_t tid = pthread_self();
    int sem_id = 1;
    char input[] = "Test Input";
    int remaining_time = 10;

    node *n = create_node(tid, sem_id, input, remaining_time);
    assert(n != NULL);
    assert(n->thread_id == tid);
    assert(n->semaphore_id == sem_id);
    assert(strcmp(n->input, input) == 0);
    assert(n->remaining_time == remaining_time);
    assert(n->next == NULL);

    free(n);
    printf("Test Create Node: Passed\n");
}

void test_tail_insert() {
    node *head = NULL;
    node *n = create_node(pthread_self(), 2, "Tail Node", 5);
    tail_insert(&head, n);

    assert(head == n);
    assert(head->next == NULL);

    // Clean up
    free(n);
    printf("Test Tail Insert: Passed\n");
}


void test_head_insert() {
    node *head = NULL;
    node *n1 = create_node(pthread_self(), 3, "First Node", 10);
    head_insert(&head, n1);
    node *n2 = create_node(pthread_self(), 4, "Second Node", 15);
    head_insert(&head, n2);

    assert(head == n2);
    assert(head->next == n1);

    // Clean up
    free(n1);
    free(n2);
    printf("Test Head Insert: Passed\n");
}

void test_delete_node() {
    node *head = NULL;
    pthread_t tid = pthread_self();
    node *n = create_node(tid, 5, "Delete Test", 20);
    tail_insert(&head, n);

    delete_node(&head, tid);
    assert(head == NULL);

    printf("Test Delete Node: Passed\n");
}

void test_search_node() {
    node *head = NULL;
    pthread_t tid = pthread_self();
    node *n = create_node(tid, 6, "Search Test", 25);
    tail_insert(&head, n);

    node *found = search_node(head, tid);
    assert(found == n);

    // Clean up
    free(n);
    printf("Test Search Node: Passed\n");
}

void test_print_functions() {
    node *head = NULL;
    node *n = create_node(pthread_self(), 7, "Print Test", 30);
    tail_insert(&head, n);

    printf("Testing Print Node:\n");
    print_node(n);
    printf("\nTesting Print List:\n");
    print_list(head);

    // Clean up
    free(n);
}

int main() {
    test_create_node();
    test_tail_insert();
    test_head_insert();
    test_delete_node();
    test_search_node();
    test_print_functions();

    return 0;
}
*/