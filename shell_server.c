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
#include <semaphore.h>
#include <math.h>
#include <limits.h>

#include "linked_list.h"
# include "execute_command.h"

#define PORT 6000

sem_t sem_client[5]; // 5 client --> 5 semaphores
int client_flag = 0; // the index to indicate which semaphore the client is using
node *my_list; // Single Linked list
// NEW!!!  the argument passed in the client thread
struct client_arg{
   int new_socket; //socket for inter-process communications
   node* my_list; // Single Linked list
   int my_flag; // which semaphore the client uses
} *args;

void *handle_client(void *arg); 
void *manage_process(void *link_list); // NEW!!! manage the process

int main()
{
	my_list = create_node(0, -1,"header",0,0,0,0); // Initialize the linked list
    
	// NEW!!! manager thread----------------------------------------------------
    pthread_t thread_id;
    int rc; // return value for pthread_create
    rc = pthread_create(&thread_id, NULL, manage_process, my_list); // create a thread to manage the process
    if (rc) // if rc is > 0 imply could not create new thread
    {
      printf("\n ERROR: return code from pthread_create is %d \n", rc);
      exit(EXIT_FAILURE);
    }
	// ------------------------------------------------------------------------
	
	//create socket
	int server_socket;
	server_socket = socket(AF_INET , SOCK_STREAM,0);
	// set socket option to reuse the address
	if(setsockopt(server_socket, SOL_SOCKET,SO_REUSEADDR,&(int){1},sizeof(int))<0)
    { // error checking for setsockopt
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
	while (1) 
	{
		//listen for connections, and allow 5 connections to wait in queue
		if(listen(server_socket,5)<0)
		{ // error checking for listen
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
		// NEW!!! initialize the semaphore----------------------------------------
		// the semaphore is initialized to 0, so that the client thread will wait for the manager thread to post the semaphore
		sem_init(&sem_client[client_flag],0,0); 
		//----------------------------------------------------------------------
		//NEW!!! create the argument for the thread
		args = malloc(sizeof(struct client_arg) * 1); // allocate memory for the argument
		if (args == NULL) // error checking for malloc
		{
			perror("Failed to allocate memory");
			exit(EXIT_FAILURE);
		}
		// initialize the argument
    	args->new_socket = client_socket;
    	args->my_list = my_list;
    	args->my_flag = client_flag;
		pthread_t thread; // create a thread for each client [client thread, run handle_client]
		if (pthread_create(&thread, NULL, handle_client, args) != 0) 
		{ // error checking for pthread_create
            perror("Error creating thread");
            exit(EXIT_FAILURE);
        }
		client_flag++; // increment the client_flag, so that the next client will use the next semaphore
		
		//----------------------------------------------------------------------

	}
    close(server_socket); // Close the socket

    return 0;
}
	
// NEW!!! manage the process
void *manage_process(void *link_list)
{
	//these are the utilities node to schedule the process
	node *header = link_list; 
	node *next_node = my_list->next;
	node *current = header;
	node *node_to_begin = header;
	node *node_last_second = header;
	node *min_node = header;
	node *second_min_node = header;
	node *node_last_round = header;

	int min_time;
	int second_min_time;

	int client_socket; 
	int finish_flag = 0; //this is the flag to indicate whether the process is finished

	// a while loop to continuously check the linked list every second for incoming commands
	// this is necessary to ensure the new shortest commands (including shell commands) can be executed immediately
	while(1)
	{
		//reset the flag if previously set to 1
		finish_flag = 0;
		//reset the node_to_begin to the first node after header
		next_node = my_list->next;
		if(next_node == NULL)
		{ // if no command, skip this cycle
            sleep(1); // sleep for 1 second, then check again [we check the new command every second]
            continue;
        }

		node_to_begin = next_node; // set the node_to_begin to the first node after header

		if(next_node->remaining_time == -1)// if the command is a shell command
		{ 
			sem_post(&sem_client[next_node->semaphore_id]);//post the semaphore and allow the command to run
			delete_node(&my_list, next_node->thread_id);//delete the node from the linked list and forget about it
			finish_flag = 1;//set the flag to 1 to indicate the process is finished
		}
		else// if the command is a program
		{ 
			min_time = INT_MAX;	// initialize the min_time to the maximum integer value
			second_min_time = INT_MAX;	// initialize the second_min_time to the maximum integer value
			// initilize the min_node and second_min_node to the next_node, which is the first node after header
			// this is necessary to ensure the min_node and second_min_node are not NULL
			current = next_node;
			min_node = next_node;
			second_min_node = next_node;

			//find the shortest time and the second shortest time
			while (current != NULL) // loop through the linked list
			{
				if (current->remaining_time < min_time) 
				{
					// to find the shortest time
					second_min_time = min_time;
					second_min_node = min_node;
					
					min_time = current->remaining_time;
					min_node = current;
				} else if (current->remaining_time <= second_min_time && current->remaining_time != min_time) 
				{ 
					// to find the second shortest time
					second_min_time = current->remaining_time;
					second_min_node = current;
				}
				// move to the next node
				current = current->next;
				
			}

			// the length of the list greater than 3 ensures that there are at least 2 nodes after header
			// this is necessary to handle the situation for not same progress[program+program] be selected in two consecutive rounds
			// shell+program is aleady handled by the if statement above
			if (get_list_length(header)>=3)
			{

				// a special case where the two shortest nodes have just finished there former cycle
				if (min_node->runned_time ==0 && second_min_node->runned_time ==0) 
				{
					if (min_node->client_id != node_last_round->client_id)//therefore, if the min_node is not the node_last_round, prioritize it
					{
						node_to_begin = min_node;
					}
					else if (min_node->client_id == node_last_round->client_id )//give the priority to the second_min_node if the min_node is the node_last_round
					{
						node_to_begin = second_min_node;
					}
					
				}

				// a more general case where the min_node is not the node_last_round, apply the SJRF algorithm
				else if (min_node->client_id != node_last_round->client_id)
				{
					node_to_begin = min_node;
				}
				else//if the min_node is the node run last round unfortunately
				{
					if (node_last_second->runned_time!=0)//if the min_node (node_last_round) ended naturally (not preempted)
					{
						if (node_last_round->client_id == min_node->client_id)
						{
							node_to_begin = second_min_node;
						}
						else
						{
							node_to_begin = min_node;
						}
					}
					else//if the min_node (node_last_round) ended preempted
					{
						if (node_last_round->client_id == min_node->client_id)
						{
							node_to_begin = second_min_node;
						}
						else
						{
							node_to_begin = min_node;
						}
					}
				}
				
			}
			// the code above is for scheduling the process to run
			// the code below is for updating the info for the nodes in the linked list and release the semaphore for the process to run
			if (node_to_begin->remaining_time <= 1) // if the program is about to complete (just one second left!)
			{ 
				sem_post(&sem_client[node_to_begin->semaphore_id]); // post semaphore to let the process run
				finish_flag = 1;//set the flag to 1 to indicate the process is finished
			}
			else// if the program is not completed (more seconds left to run!)
			{ 
				if (node_to_begin->client_id != node_last_second->client_id)//if the current running node is not the same as the node run last second
				{
					
					//we need to notify the node run last second that it was preempted to move on!
					if (node_last_second->runned_time!=0)//however, this is only needed if the node run last second ended preempted
					{
						node_last_second->round +=1;//increment the round for the node run last second
						node_last_second->runned_time = 0;//reset the runned_time for the node run last second to the beginning of next round
					}
					
					//if the node las second was preempted, we need to update the node_last_round
					if (node_last_second->round ==1 && node_last_second->runned_time < 3 || node_last_second->round >=2 && node_last_second->runned_time < 7)
					{
						node_last_round = node_last_second;
					}

					//also we need to update the node_last_second to the current node
					node_last_second = node_to_begin;
				}
				
				//if the node is running during the 2,3,4... round
				if (node_to_begin->round >=2 && node_to_begin->runned_time < 7)
				{
					node_to_begin->runned_time++;//increment the runned_time for simulation purpose
				}
				//if the node is running during the 2,3,4... round and finishes naturally
				if (node_to_begin->round >=2 && node_to_begin->runned_time >= 7)
				{
					node_to_begin->round +=1;//increment the round for the node
					node_to_begin->runned_time = 0;//reset the runned_time for the node to the beginning of next round
					node_last_round = node_to_begin;//update the node_last_round to the current node so that the node next second would know this info
				}

				//if the node is running during the first round
				if (node_to_begin->round == 1 && node_to_begin->runned_time < 3)
				{
					node_to_begin->runned_time++;
				}
				
				//if the node is running during the first round and finishes naturally
				if (node_to_begin->round == 1 && node_to_begin->runned_time >= 3)
				{
					node_to_begin->round +=1;//increment the round for the node
					node_to_begin->runned_time = 0;//reset the runned_time for the node to the beginning of next round
					
					node_last_round = node_to_begin;//update the node_last_round to the current node so that the node next second would know this info
				}
				
				
				
			}
			node_to_begin->remaining_time--; // decrement the remaining time for simulation purpose
			node_to_begin->total_progress++; // increment the total time run for simulation purpose

			client_socket = node_to_begin->socket;

			// if this is the demo program, we prepare the output to the client
			if (node_to_begin->input[0]=='.' && node_to_begin->input[1]=='/' &&node_to_begin->input[2]=='d'&&node_to_begin->input[3]=='e'&&node_to_begin->input[4]=='m'&&node_to_begin->input[5]=='o')
			{
				char buffer[256];
				int length = snprintf(buffer, sizeof(buffer), "demo: %d/%d\n", node_to_begin->total_progress, node_to_begin->total_time);
				send(client_socket, buffer, length, 0);
				
			}

			printf("################# demo %d/%d #######################\n", node_to_begin->total_progress, node_to_begin->total_time);

			sleep(1); // sleep for 1 second for simulation purpose

			if (finish_flag!=1)//if the process has not finished
			{ 
				sem_wait(&sem_client[node_to_begin->semaphore_id]); // wait for the client to finish
				node_last_second = node_to_begin;//update the node_last_second to the current node
				printf("------------------------------------------------------------------\n");
				print_list(header);
				
			}
			else//if the process has finished
			{
				send(client_socket, "finish", sizeof("finish"), 0);//send the finish signal to the client so that the client can ask for user input again
				
				node_last_second = header;//reset the node_last_second to the header
				node_last_round = header;//reset the node_last_round to the header
				//delete the node from the linked list
				delete_node(&my_list, node_to_begin->thread_id);
			}
		}	
	}
}

// NEW!!! new version
void *handle_client(void *args)
{
	struct client_arg *arguments = args; // cast the argument to the struct client_arg
	int client_socket = arguments->new_socket; // get the socket from the argument
	pthread_t thread_id = pthread_self(); // get the thread id
	int flag = arguments->my_flag; // get the semaphore id from the argument

	printf("[%d]<<< client connected\n", flag);

	while (1) 
	{ // loop for continuously receiving commands
        char command[1024];
    	ssize_t bytes_received=recv(client_socket, command, sizeof(command), 0);
		if (bytes_received <= 0) 
		{ // error checking for recv
            // The client has disconnected.
			printf("[%d]<<< client disconnected\n", flag);
			// NEW!!! add delete process with the client id when the client disconnects!
			while(search_node(arguments->my_list, pthread_self())!=NULL){
				delete_node(&arguments->my_list, pthread_self());
			}
			close(client_socket);
            break;
        }
		// Null terminate the string to avoid printing garbage.
		command[bytes_received] = '\0';
        if (strncmp("exit", command, 4) == 0||strncmp("quit", command, 4) == 0) { // Exit the shell if the user enters "exit" or "quit"
            printf("[%d]<<< client disconnected\n", flag);
			// NEW!!! add delete process with the client id when the client disconnects!
			while(search_node(arguments->my_list, pthread_self())!=NULL){
				delete_node(&arguments->my_list, pthread_self());
			}
        	break;
    	}
		printf("[%d]>>> %s\n", flag, command);

		//NEW!!! add the command to the linked list --------------------------------------------------------------------------------
		int remaining_time = 0;
		if (strstr(command, "./demo") != NULL)
		{ // if the command is a demo program
			printf("in thread %p: this is the demo program\n", (void*)thread_id);
			sscanf(command, "./demo %d", &remaining_time); // get the remaining time from the command
			printf("[%d]--- created (%d)\n", flag, remaining_time); 
			tail_insert(&arguments->my_list, create_node(thread_id, flag, command, remaining_time, 1,0, client_socket));
			sem_wait(&sem_client[flag]); // wait for the manager thread to post the semaphore
		}
		else if(strstr(command, "./") != NULL)
		{	// if the command is a program
			printf("in thread %p: this is the rest program\n", (void*)thread_id);
			remaining_time =1;
			printf("[%d]--- created (%d)\n", flag, remaining_time);
			tail_insert(&arguments->my_list, create_node(thread_id, flag, command, remaining_time, 1,0, client_socket));
			sem_wait(&sem_client[flag]);
			execute_command(command, client_socket);//execute the command and output in the file
		}
		else 
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
		

		
		// -------------------------------------------------------------------------------------------------------------------
	}
	close(client_socket);
	free(arguments);
	pthread_exit(NULL);
}
