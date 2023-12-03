#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <pthread.h>

// Define the struct for a unit in the single linked list
typedef struct node 
{
    pthread_t thread_id; // Client's thread id
    int semaphore_id; // Semaphore id
    char input[1024]; // The message the client input
    int remaining_time; 
    // The shell command's time will be set to -1
    // The program's time will decrement by 1 every second
    int round; // The round of the client
    int runned_time; // The runned time of the client
    int total_time;
    int total_progress;
    struct node *next;
    int socket;
    int client_id;
} node;

// Function prototypes
node* create_node(pthread_t thread_id, int semaphore_id, char input[1024], int remaining_time,int round,int runned_time,int socket);
void tail_insert(node **head, node *new_node);
void head_insert(node **head, node *new_node);
void delete_node(node **head, pthread_t thread_id);
node* search_node(node *head, pthread_t thread_id);
void print_node(const node *n);
void print_list(const node *head);
int get_list_length(const node *head);

#endif // LINKED_LIST_H
