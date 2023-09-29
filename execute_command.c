#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64

// Function to execute a command
int execute_command(char *command) {
    // Execute the command
    char *args[MAX_ARGS];
    int arg_count = 0;
    // put the input command into args
    // this version currently only support single command, no pipe
    char *component_of_command = strtok(command, " ");
    while (component_of_command != NULL) {
        args[arg_count] = component_of_command;
        arg_count++;
        component_of_command = strtok(NULL, " ");
    }
    args[arg_count] = NULL;
    pid_t pid;
    int status;
    
    pid = fork();
    if (pid == 0) {
        // Child process
        // check whether the command is ls,pwd,mkdir
        // should add more basic commands
        if (strcmp(args[0], "ls") == 0){
            execvp("ls", args);
            perror("Error: ");
            exit(EXIT_FAILURE);
        }
        else if (strcmp(args[0], "pwd") == 0){
            execvp("pwd", args);
            perror("Error: ");
            exit(EXIT_FAILURE);
        }
        else if (strcmp(args[0], "mkdir") == 0){
            execvp("mkdir", args);
            perror("Error: ");
            exit(EXIT_FAILURE);
        // this is the last one, handling the command not found error
        }else{
            printf("Command not found\n");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        // Fork failed
        perror("fork");
    } else {
        // Parent process
        wait(&status);
        } 

    return 1; // Return 1 to continue the shell
}