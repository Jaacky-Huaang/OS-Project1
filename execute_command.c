#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define MAX_CMDS 4

char **create_args(char *command, char *seperator){
    char **args = (char **)malloc(MAX_ARGS * sizeof(char *));
    if (!args) {
        perror("Failed to allocate memory for args");
        exit(EXIT_FAILURE);
    }

    int arg_count = 0;
    char *component = strtok(command, seperator);
    while (component != NULL && arg_count < MAX_ARGS - 1) {
        args[arg_count] = component;
        arg_count++;
        component = strtok(NULL, seperator);
    }
    args[arg_count] = NULL;

    return args;
}


//a recursive function to execute multiple commands linked by "|"
//n: the number of commands

int execute_command(char *command) {

    //if the command contains "|", it means that it involves piping
    if (strchr(command, ' | ')) {
        execute_piped_commands(command);
    }

    if (strchr(command, ' > ')) {
        
        redirecting_input(command);
    }

    if (strchr(command, ' < ')) {
        
        redirecting_output(command);
    }

    //the current command does not involve piping
    else{
        execute_single_command(command);
    }
}

int execute_single_command(char *command) {

    char **args = create_args(command, " ");
    pid_t pid;
    int status;
    
    pid = fork();
    if (pid == 0) {
        // Child process
        // check whether the command is ls,pwd,mkdir
        // should add more basic commands

        /*
        if (input_fd != STDIN_FILENO) {  // Check if the input_fd is not standard input
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);  // Close the original file descriptor after duplication
        }

        if (output_fd != STDOUT_FILENO) {  // Check if the output_fd is not standard output
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);  // Close the original file descriptor after duplication
        }
        */

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
        perror("fork failed");
    } else {
        // Parent process
        wait(&status);
        } 

    return 1; // Return 1 to continue the shell
}

int execute_piped_commands(char* command) {

    char *cmds[MAX_CMDS];
    int cmd_count = 0;
    
    char *component_of_command = strtok(command, " | ");
    while (component_of_command != NULL && cmd_count < MAX_CMDS) {
        cmds[cmd_count] = component_of_command;
        cmd_count++;
        component_of_command = strtok(NULL, " | ");
    }
    //create the pipe file descriptor to communicate between two adjacent commands
    int pipefd[2];
    pid_t pid;

    //Base case for the recursion
    /*
    if (n==1){

        //We first create the args list
        char **args = create_args(piped_commands[0], " ");

        //Then we create the pipe
        if (pipe(pipefd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        //Then we fork the process to create a child
        pid = fork();

        //If the fork fails
        if (pid<0){
            perror("fork failed");
        }

        //If we are in the child process
        if (pid==0){

        }
    }*/
}

int redirecting_output(char *command) {
    char *outfile = NULL;
    char *errfile = NULL;
    char *redirect = strstr(command, ">");
    char *redirect_err = strstr(command, "2>");

    if (redirect) {
        *redirect = '\0';
        outfile = strtok(redirect + 1, " ");
        outfile += strspn(outfile, " \t");
    }

    if (redirect_err) {
        *redirect_err = '\0';
        errfile = strtok(redirect_err + 2, " ");
        errfile += strspn(errfile, " \t");
    }

    char **args = create_args(command);
    
    int status;
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        if (outfile) {
            int fd_out = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out < 0) {
                perror("Failed to open the output file");
                exit(EXIT_FAILURE);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }

        if (errfile) {
            int fd_err = open(errfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_err < 0) {
                perror("Failed to open the error file");
                exit(EXIT_FAILURE);
            }
            dup2(fd_err, STDERR_FILENO);
            close(fd_err);
        }
        
        execvp(args[0], args);
        perror("Command execution failed");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        wait(&status);
    }
    return 0;
}

int redirecting_input(char *command) {
    char *infile = NULL;
    char *redirect = strstr(command, "<");

    if (redirect) {
        *redirect = '\0';
        infile = strtok(redirect + 1, " ");
        infile += strspn(infile, " \t");
    }

    char **args = create_args(command);
    
    int status;
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        if (infile) {
            int fd_in = open(infile, O_RDONLY);
            if (fd_in < 0) {
                perror("Failed to open the input file");
                exit(EXIT_FAILURE);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }
        
        execvp(args[0], args);
        perror("Command execution failed");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        wait(&status);
    }
    return 0;
}