#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
# include "execute_command.h"

#define PORT 6000

int main()
{
	//create socket
	int server_socket;
	server_socket = socket(AF_INET , SOCK_STREAM,0);

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
	
	//listen for connections, and allow 5 connections to wait in queue
	if(listen(server_socket,5)<0){ // error checking for listen
		printf("Listen failed..\n");
        exit(EXIT_FAILURE);
	}
	 else
    	printf("Server: socket LISTEN success..\n");
	// loop for continuously accepting connections and executing commands
	while (1) {
		// accept a connection
		int addrlen = sizeof(server_address);	
    	int client_socket = accept(server_socket, (struct sockaddr *) &server_address, (socklen_t *)&addrlen);
    	if (client_socket < 0) { // error checking for accept
        	printf("accept failed...\n");
        	exit(EXIT_FAILURE);
    	}
    	printf("Server: socket ACCEPT success..\n");
    	while (1) { // loop for continuously receiving commands
        	char command[1024];
        	ssize_t bytes_received=recv(client_socket, command, sizeof(command), 0);
			if (bytes_received <= 0) { // error checking for recv
                // The client has disconnected.
                close(client_socket);
                break;
            }
			// Null terminate the string to avoid printing garbage.
			command[bytes_received] = '\0';
        	if (strncmp("exit", command, 4) == 0) { // Exit the shell if the user enters "exit" or "quit"
            	printf("Server Exit...\n");
            	break;
        	}
			// Execute the command and output in the server
        	printf("Server output:\n");
			execute_command(command);
			
    	}
	}
    close(server_socket); // Close the socket
    return 0;
}
	



