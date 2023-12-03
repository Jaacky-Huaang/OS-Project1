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

#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/mman.h>
#include<semaphore.h>

#include "linked_list.h"
# include "execute_command.h"

#define PORT 6000

sem_t sem_client[10]; // ten client --> ten semaphores
int client_flag = 0; // the index to indicate which semaphore the client is using

struct client_arg{
   int new_socket; //socket for inter-process communications
   node* my_list; // Single Linked list
   int my_flag; // which semaphore the client uses
} *args;

void *handle_client(void *arg);
void *manage_process(void *link_list);
int main()
{
	//create socket
	node *my_list = create_node(0, -1,"header",0,0,0,0); // Initialize the linked list
    // NEW!!! manager thread
    pthread_t thread_id;
    int rc;
    rc = pthread_create(&thread_id, NULL, manage_process, my_list);
    if (rc) // if rc is > 0 imply could not create new thread
    {
      printf("\n ERROR: return code from pthread_create is %d \n", rc);
      exit(EXIT_FAILURE);
    }
	int server_socket;
	server_socket = socket(AF_INET , SOCK_STREAM,0);

	if(setsockopt(server_socket, SOL_SOCKET,SO_REUSEADDR,&(int){1},sizeof(int))<0)
    {
        perror("setsockopt error");
        exit(EXIT_FAILURE);
    }

	//check for fail error
	if (server_socket == -1) {
        printf("socket creation failed..\n");
        exit(EXIT_FAILURE);
    }
    else
    	printf("Server: socket CREATION success..\n");
    

	//define server address structure
	struct sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PORT);
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);


	//bind the socket to our specified IP and port
	if (bind(server_socket , 
		(struct sockaddr *) &server_address,
		sizeof(server_address)) < 0)
	{
		printf("socket bind failed..\n");
        exit(EXIT_FAILURE);
	}
	 else
    	printf("Server: socket BIND success..\n");
	
	// loop for continuously accepting connections and executing commands
	while (1) {
		//listen for connections, and allow 5 connections to wait in queue
		if(listen(server_socket,5)<0){ // error checking for listen
		printf("Listen failed..\n");
        exit(EXIT_FAILURE);
		}
	 	else
    		printf("Server: socket LISTEN success..\n");
		// accept a connection
		int addrlen = sizeof(server_address);	
    	int client_socket = accept(server_socket, (struct sockaddr *) &server_address, (socklen_t *)&addrlen);
    	if (client_socket < 0) { // error checking for accept
        	printf("accept failed...\n");
        	exit(EXIT_FAILURE);
    	}
    	printf("Server: socket ACCEPT success..\n");
		// Create a thread to handle the client
		sem_init(&sem_client[client_flag],0,0); // NEW!!! initialize the semaphore
		//NEW!!! create the argument for the thread
		args = malloc(sizeof(struct client_arg) * 1);
    	args->new_socket = client_socket;
    	args->my_list = my_list;
    	args->my_flag = client_flag;
		pthread_t thread;
		if (pthread_create(&thread, NULL, handle_client, args) != 0) {
            perror("Error creating thread");
            exit(EXIT_FAILURE);
        }
		client_flag++;

	}
    close(server_socket); // Close the socket
    return 0;
}
	
// NEW!!! manage the process
void *manage_process(void *link_list){
	node *my_list = (node*)link_list;
	printf("main thread starts working!\n");
	node *next_node = my_list->next;
	
	int finish_flag = 0;
	while(1){
		finish_flag = 0;
		next_node = my_list->next;
		if(next_node == NULL){ // if no command, print no command
            printf("Nothing in the queue now, waiting for commands.\n");
            sleep(1);
            continue;
        }
		node *node_to_begin = next_node;
		node *prev_run=node_to_begin;
		if(next_node->remaining_time == -1){ // if the command is a shell command
			sem_post(&sem_client[next_node->semaphore_id]);
			delete_node(&my_list, next_node->thread_id);
			finish_flag = 1;
			}
		else{ // if the command is a program
			int min_time = next_node->remaining_time;
			node *current = next_node;
			while (current != NULL){ // find the node with the minimum remaining time
        		if (current->remaining_time <min_time){
					min_time = current->remaining_time;
					node_to_begin=current;
				}
				current = current->next; 
        	}
			node_to_begin->total_progress++;
			if (node_to_begin->remaining_time <= 1){ // if the remaining time is 1, delete the node and the execution ends
				sem_post(&sem_client[node_to_begin->semaphore_id]);
				finish_flag = 1;
				delete_node(&my_list, node_to_begin->thread_id);
			}
			else{ // if the remaining time is not 1, decrement the remaining time by 1
				sem_post(&sem_client[node_to_begin->semaphore_id]);
				node_to_begin->remaining_time--; 
				if (node_to_begin!=prev_run){
					prev_run->round++; // if the node is not the previous node, increment the round of the previous node 
				}
				else if (node_to_begin->round == 1){
					if (node_to_begin->runned_time==3){
						node_to_begin->runned_time=0;
						prev_run->round++;
					}
					else if(node_to_begin->runned_time<3){
						node_to_begin->runned_time++;
					}	
				}
				else if (node_to_begin->round>1){
					if (node_to_begin->runned_time==7){
						node_to_begin->runned_time=0;
						prev_run->round++;
					}
					else if(node_to_begin->runned_time<7){
						node_to_begin->runned_time++;
					}	
				}
			} 	
			}
		char buffer[256]; // 根据需要的大小调整缓冲区
		int length = snprintf(buffer, sizeof(buffer), "demo: %d/%d\n", node_to_begin->total_progress, node_to_begin->total_time);

		printf("after this second: demo %d/%d\n", node_to_begin->total_progress, node_to_begin->total_time);

		// get the client socket
		int client_socket = node_to_begin->socket;
		send(client_socket, buffer, length, 0); // 将字符串发送给客户端
		
		sleep(1); // sleep for 1 second
		if (finish_flag!=1){ // if the node is not completed, check again
			sem_wait(&sem_client[node_to_begin->semaphore_id]); // wait for the client to finish
			prev_run=node_to_begin; // prev_run should be the node which runs, this is the value when the node is not completed
		}
		else{
			send(client_socket, "finish", sizeof("finish"), 0);
			prev_run=next_node; // prev_run should be the node which runs, this is the value when the node is completed
		}
	}	
}

// NEW!!! new version
void *handle_client(void *args)
{
	struct client_arg *arguments = args;
	int client_socket = arguments->new_socket; // dereference socket
	pthread_t thread_id = pthread_self();
	int flag = arguments->my_flag; 

	printf("[%d]<<< client connected\n", flag);

	while (1) 
	{ // loop for continuously receiving commands
        char command[1024];
    	ssize_t bytes_received=recv(client_socket, command, sizeof(command), 0);
		if (bytes_received <= 0) 
		{ // error checking for recv
            // The client has disconnected.
			printf("[%d]<<< client connected\n", flag);
			// NEW!!! add delete process with the client id!
			while(search_node(arguments->my_list, pthread_self())!=NULL){
				delete_node(&arguments->my_list, pthread_self());
			}
			close(client_socket);
            break;
        }
		// Null terminate the string to avoid printing garbage.
		command[bytes_received] = '\0';
        if (strncmp("exit", command, 4) == 0||strncmp("quit", command, 4) == 0) { // Exit the shell if the user enters "exit" or "quit"
            printf("[%d]<<< client connected\n", flag);
			// NEW!!! add delete process with the client id!
			while(search_node(arguments->my_list, pthread_self())!=NULL){
				delete_node(&arguments->my_list, pthread_self());
			}
        	break;
    	}
		printf("[%d]>>> %s\n", flag, command);

		//NEW!!! add the command to the linked list --------------------------------------------------------------------------------
		int remaining_time = 0;
		if (command[0]!='.' && command[1]!='/')
		{
			head_insert(&arguments->my_list, create_node(thread_id, flag, command, -1, 1,0, client_socket));
			printf("[%d]--- created (%d)\n", flag, -1);
			sem_wait(&sem_client[flag]);
			printf("[%d]--- started (%d)\n", flag, -1);
			execute_command(command, client_socket);//execute the command and output in the file
			printf("[%d]<<< output sent (%d)\n", flag, -1);
			send(client_socket, "finish", sizeof("finish"), 0);
			printf("[%d]--- ended (%d)\n", flag, -1);
		}
		else if(command[0]=='.' && command[1]=='/' &&command[2]!='d'&&command[3]!='e'&&command[4]!='m'&&command[5]!='o')
		{	
			printf("in thread %p: this is the rest program\n", (void*)thread_id);
			remaining_time =1;
			printf("[%d]--- created (%d)\n", flag, remaining_time);
			tail_insert(&arguments->my_list, create_node(thread_id, flag, command, remaining_time, 1,0, client_socket));
			sem_wait(&sem_client[flag]);
			execute_command(command, client_socket);//execute the command and output in the file
		}
		else if (command[0]=='.' && command[1]=='/' &&command[2]=='d'&&command[3]=='e'&&command[4]=='m'&&command[5]=='o')
		{
			printf("in thread %p: this is the demo program\n", (void*)thread_id);
			sscanf(command, "./demo %d", &remaining_time);
			printf("[%d]--- created (%d)\n", flag, remaining_time);
			tail_insert(&arguments->my_list, create_node(thread_id, flag, command, remaining_time, 1,0, client_socket));
			sem_wait(&sem_client[flag]);
		}
		
		// -------------------------------------------------------------------------------------------------------------------
	}
	close(client_socket);
	free(arguments);
	pthread_exit(NULL);
}

