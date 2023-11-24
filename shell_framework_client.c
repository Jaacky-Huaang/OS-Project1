#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
# include "execute_command.h"

#define PORT 6000
int main() {
    // Create a socket
    int client_socket;
    struct sockaddr_in server_addr;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) { // Check for error
        perror("Error creating socket");
        return 1;
    }
    else{
        printf("Client: socket CREATION success..\n");
    }    
        

    // Server configuration
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error connecting to the server");
        return 1;
    }
    // The user's input
    char input[256];
    while (1) {
        // ask for a prompt
        printf("$ ");
        scanf(" %[^\n]", input);
        // Send the command to the server
        if (send(client_socket,input, strlen(input), 0) == -1) { // Check for error
            perror("Error sending command");
            break;
        }
        // Exit the shell if the user enters "exit" or "quit"
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            printf("Exiting MyShell...\n");
            break;
        }

        // Receive the output from the server
        char output[2048];
        memset(output, 0, sizeof(output));  // initialize the buffer

        int bytes_received = recv(client_socket, output, sizeof(output) - 1, 0); 
        // leave space for a null terminator
        if (bytes_received == -1) {
            // Check for error
            perror("Error receiving output");
            break;
        } 
        else if (bytes_received == 0)
         {
            // Nothing received
            printf("No message back\n");
            //continue;
        }
        else 
        {
            output[bytes_received] = '\0'; // make sure it's null terminated
        }
        // Print the output
        printf("Server responce:\n");
        printf("%s", output);
    }
    close(client_socket); // Close the socket
    return 0;
}






