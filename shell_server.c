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



void *handle_client(void *arg);
int main()
{
	//create socket
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
		pthread_t thread;
		if (pthread_create(&thread, NULL, handle_client, &client_socket) != 0) {
            perror("Error creating thread");
            exit(EXIT_FAILURE);
        }
    	
	}
    close(server_socket); // Close the socket
    return 0;
}
	


void *handle_client(void *arg){
	int *client_socket = (int *)arg;
	int s=*client_socket; // dereference socket
	FILE *fp;
	while (1) { // loop for continuously receiving commands
        char command[1024];
    	ssize_t bytes_received=recv(s, command, sizeof(command), 0);
		if (bytes_received <= 0) { // error checking for recv
            // The client has disconnected.
            close(s);
            break;
        }
			// Null terminate the string to avoid printing garbage.
		command[bytes_received] = '\0';
        if (strncmp("exit", command, 4) == 0) { // Exit the shell if the user enters "exit" or "quit"
            printf("Client Exit...\n");
        	break;
    	}
		// Execute the command and output in the server

		fp = fopen("_temp.txt", "w+"); //open the file to capture the output
		// fp=fdopen(s,"w+");
		if (fp == NULL) 
		{
			perror("Error opening file");
			exit(EXIT_FAILURE);
		}
		execute_command(command, fileno(fp));//execute the command and output in the file
		
		fflush(fp); // Renew the file buffer
		fclose(fp); // Close the file descriptor

		// Send the output file to the client
		char buffer[2048];
		int fd = open("_temp.txt", O_RDONLY);//open the file to access the output
		if (fd < 0) {
			perror("Error opening file");
			exit(EXIT_FAILURE);
		}
			
		int bytes_read = read(fd, buffer, sizeof(buffer));//read the output from the file
		if (bytes_read <= 0) {
			break;
		}
		send(s, buffer, bytes_read, 0);//send the output to the client
		printf("Server: message SEND success..\n");
		fclose(fp); // Close the file descriptor
		
	}
	close(s);
	pthread_exit(NULL);
}

