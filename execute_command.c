#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

#define MAX_INPUT_SIZE 256 // The maximum number of characters for the user input
#define MAX_ARGS 64 // The maximum number of arguments for a command
#define MAX_CMDS 8  // Assuming a maximum of 8 commands in a pipeline


//a utility function to create arguments for the command
char **create_args(const char *command, const char *separator) {
    char **args = (char **)malloc(MAX_ARGS * sizeof(char *)); // Allocate memory for the arguments
    if (!args) { // Make sure the allocation was successful
        perror("Failed to allocate memory for args");
        exit(EXIT_FAILURE);
    }

    char *command_copy = strdup(command);//make a copy of the command because strtok will modify it
    if (!command_copy) { // Make sure the allocation was successful
        perror("Failed to copy command");
        free(args);
        exit(EXIT_FAILURE);
    }

    int arg_count = 0; // Number of arguments
    char *component = strstr(command_copy, separator);  // Find first occurrence of separator
    while (component != NULL && arg_count < MAX_ARGS - 1) // -1 to leave room for NULL
    {
        *component = '\0';//replace the separator with a null character
        args[arg_count] = command_copy;//add the component to the array of arguments
        arg_count++;//increment the number of arguments
        command_copy = component + strlen(separator);  // Move past the separator
        component = strstr(command_copy, separator);  // Find next occurrence of separator
    }

    // Append the remaining part or the entire string if no separator was found
    if (*command_copy) {
        args[arg_count++] = command_copy;
    }
    args[arg_count] = NULL;//set the last element of the array to NULL

    return args;
}

//_______________________________________________________________________________________

int input_redirect(char *command, int output_fd)
{   
    char **args = create_args(command, " < "); // create arguments for the command, input redirection so we use <
    char *sub_command = args[0]; // the command
    char *input_file = args[1];  // the input file for the redirection

    int input_fd = open(input_file, O_RDONLY);  // open the input file

    if (input_fd < 0) { // Make sure the file was opened successfully
        perror("Failed to open input file");
        exit(1);
    }

    char **sub_args = create_args(sub_command, " ");//create arguments for the command
    dup2(input_fd, STDIN_FILENO);  // duplicate file descriptor to stdin
    close(input_fd);  // close the original file descriptor

    if (output_fd != -1) 
    {   // If output_fd is valid, redirect stdout and stderr to it
        dup2(output_fd, STDOUT_FILENO);
        dup2(output_fd, STDERR_FILENO);
        close(output_fd); // Close the duplicated file descriptor
    }

    execvp(sub_args[0], sub_args);  // execute the command
    free(args); // Free the memory allocated for the arguments
    free(sub_args); 
    perror("Exec failed");  // execvp will only return if there's an error
    exit(1);
    return 0;
}



int output_redirect(char *command) {
    // Redirect output
    char **args = create_args(command, " > "); // Create arguments for the command, using > for output redirection
    if (args == NULL) {
        perror("Failed to create arguments");
        return -1;
    }

    char *sub_command = args[0]; // Subcommand
    char *output_file = args[1]; // Redirected output file

    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);  // Open the output file
    if (fd < 0) { // Ensure the file is successfully opened
        perror("Failed to open output file");
        free(args); // Free memory for arguments
        return -1;
    }

    char **sub_args = create_args(sub_command, " ");
    if (sub_args == NULL) {
        perror("Failed to create sub-arguments");
        close(fd);
        free(args);
        return -1;
    }

    pid_t pid = fork(); // Create a child process
    if (pid < 0) {
        perror("Failed to fork");
        close(fd);
        free(args);
        free(sub_args);
        return -1;
    }

    if (pid == 0) { // Child process
        dup2(fd, STDOUT_FILENO);  // Duplicate file descriptor to stdout
        close(fd);  // Close the original file descriptor

        if (execvp(sub_args[0], sub_args) < 0) {  // Execute the command
            perror("Exec failed");
            exit(EXIT_FAILURE);  // Exit the child process if execvp fails
        }
    } else { // Parent process
        int status;
        close(fd); // Close the file descriptor in the parent process
        waitpid(pid, &status, 0);  // Wait for the child process to finish
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            perror("Sub-command execution failed");
            free(args);
            free(sub_args);
            return -1;
        }
    }

    free(args); // Free memory
    free(sub_args);
    return 0;  // Successfully executed
}


int error_redirect(char *command) 
{   
    
    char **args = create_args(command, " 2> "); // create arguments for the command, error redirection so we use 2>
    char *sub_command = args[0]; // the command
    char *error_file = args[1]; // the error file for the redirection

    int fd = open(error_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);  // open the input file, googled convention
    if (fd < 0) { // Make sure the file was opened successfully
        perror("Failed to open error file");
        exit(1);
    }

    char **sub_args = create_args(sub_command, " "); // create arguments for the command
    dup2(fd, STDERR_FILENO);  // Redirect stderr to the error file
    close(fd);  
    close(STDOUT_FILENO);  // Close stdout
    execvp(sub_args[0], sub_args);  // execute the command
    free(args); // Free the memory allocated for the arguments
    free(sub_args);
    perror("Exec failed");  
    exit(1);
    return 0;
}

// function to execute the single command[no pipe, one redirection or no redirection], designed to be reused in the pipe function
int execute_helper(char *command, int output_fd) 
{

    if (strchr(command, '<') )//if the command contains <, then it is an input redirection
    {
        return input_redirect(command, -1); // redirect input
    }

    else if (strstr(command, "2>") ) // if the command contains 2>, then it is an error redirection
    {
        return error_redirect(command); // Redirect stderr to the error file
    }

    else if (strchr(command, '>')  ) // if the command contains >, then it is an output redirection
    {
        return output_redirect(command); // redirect output
    }

    else // if the command does not contain any redirection, then we just execute the command
    { 
        char **args = create_args(command, " ");//if we do not have >, <, 2>, then we just create arguments for the command, split based on space
        if (execvp(args[0], args) == -1) { // Execute the command
            write(output_fd, "Error executing command\n", 24);
            exit(EXIT_FAILURE); // Only the child exits
        }

        free(args); // Free the memory allocated for the arguments
    }
    return 1; // Return 1 to continue the shell
}

// function to execute the single command[no pipe, one redirection or no redirection], designed to be reused in the pipe function
int new_execute_helper(char *command, int output_fd)
{
    char msg[] = "Output redirection, no responce from server\n";
    char msg2[] = "Error executing output redirection: invalid command\n";
    if (strchr(command, '<') )//if the command contains <, then it is an input redirection
    {
        
        return input_redirect(command, output_fd); // redirect input
    }

    else if (strstr(command, "2>") ) // if the command contains 2>, then it is an error redirection
    {
        //leave the msg in the output file
        write(output_fd, msg, strlen(msg));
        return error_redirect(command); // Redirect stderr to the error file
    }

    else if (strchr(command, '>')  ) // if the command contains >, then it is an output redirection
    {
        int output_status = output_redirect(command); // redirect output
        if (output_status == -1)
        {
            write(output_fd, msg2, strlen(msg2));
        }
        else
        {
            write(output_fd, msg, strlen(msg));
        }
        return output_status;
    }

    else if (output_fd != -1)// If output_fd is valid, redirect stdout and stderr to it
    {
        dup2(output_fd, STDOUT_FILENO);
        dup2(output_fd, STDERR_FILENO);

        char **args = create_args(command, " ");//if we do not have >, <, 2>, then we just create arguments for the command, split based on space
        if (execvp(args[0], args) == -1) { // Execute the command
            perror("Error executing command");
            exit(EXIT_FAILURE); // Only the child exits
        }

        free(args); // Free the memory allocated for the arguments
        close(output_fd); // Close the duplicated file descriptor
    }

    else // if the command does not contain any redirection, then we just execute the command
    { 
        char **args = create_args(command, " ");//if we do not have >, <, 2>, then we just create arguments for the command, split based on space
        if (execvp(args[0], args) == -1) { // Execute the command
            perror("Error executing command");
            exit(EXIT_FAILURE); // Only the child exits
        }

        free(args); // Free the memory allocated for the arguments
    }
    return 1; // Return 1 to continue the shell
}

//command
//execute a single command without any pipes or just maximum one redirection
int execute_single_command(char *command, int output_fd) 
{

    pid_t pid = fork();//fork a child process so not to interrupt the main process
    if (pid < 0) { // Error forking
        perror("fork failed");
        exit(EXIT_FAILURE);
    } 
    if (pid == 0) { // Child process
        new_execute_helper(command, output_fd);
    } else { // Parent process
        wait(NULL); // Wait for child process to finish
    }
    return 0;
}

//command1 | command2
int execute_one_pipe(char *command1, char *command2, int output_fd) {
    
    int pipefd[2];//create a pipe

    if (pipe(pipefd) == -1) { // Make sure the pipe was created successfully
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();//fork a child process so not to interrupt the main process

    if(pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0) { // Child process for command1
        close(pipefd[0]);//close the read end of the pipe
        dup2(pipefd[1], STDOUT_FILENO);//replace stdout with the write end of the pipe
        execute_helper(command1, output_fd);//execute command1
    }

    close(pipefd[1]); // Close the write end of the pipe

    pid_t pid2 = fork(); // Fork another child process for command2

    if (pid2 == 0) { // Child process for command2
        close(pipefd[1]);//close the write end of the pipe
        dup2(pipefd[0], STDIN_FILENO);//replace stdin with the read end of the pipe
        new_execute_helper(command2, output_fd);//execute command2
        
    }

    close(pipefd[0]);//close the read end of the pipe

    wait(NULL); // Wait for first child
    wait(NULL); // Wait for second child

    return 0;
}

// command1 | command2 | command3
int execute_two_pipes(char *command1, char *command2, char *command3, int output_fd) {
   int pipefd1[2]; // create two pipes
   int pipefd2[2];
   if (pipe(pipefd1) == -1) { // Make sure the pipe was created successfully
        perror("pipe1 failed error");
        exit(EXIT_FAILURE);
    }
    if (pipe(pipefd2) == -1) { // Make sure the pipe was created successfully
        perror("pipe2 failed error");
        exit(EXIT_FAILURE);
    }
    pid_t child1=fork(); // fork a child process so not to interrupt the main process
    if(child1==-1){ // fork error
        perror("fork1 failed error");
        exit(EXIT_FAILURE);
    }
    else{
        if(child1==0){ // child process for command1
        dup2(pipefd1[1], STDOUT_FILENO);
        close(pipefd1[1]); // Close the write end of pipe1
        close(pipefd1[0]); // Close the read end of pipe1
        close(pipefd2[0]); // Close the read end of pipe2
        close(pipefd2[1]); // Close the write end of pipe2
        execute_helper(command1, output_fd); // execute command1
        perror("child1 execution failed error"); // error handling
        exit(EXIT_FAILURE);
        }
        else{
            pid_t child2=fork(); // fork another child process for command2
            if(child2==-1){ // fork error
                perror("fork2 failed error");
                exit(EXIT_FAILURE);
            }
            if(child2==0){ // child process for command2
            dup2(pipefd1[0], STDIN_FILENO); // replace stdin with the read end of pipe1
            dup2(pipefd2[1], STDOUT_FILENO); // replace stdout with the write end of pipe2
            close(pipefd1[1]); // Close the write end of pipe1
            close(pipefd2[0]); // Close the read end of pipe2
            close(pipefd2[1]); // Close the write end of pipe2
            close(pipefd1[0]); // Close the read end of pipe1
            execute_helper(command2, output_fd); // execute command2
            perror("child2 execution failed error"); // error handling
            exit(EXIT_FAILURE);
            }
            else{
                pid_t child3=fork(); // fork another child process for command3
                if(child3==-1){ // fork error
                    perror("fork3 failed error");
                    exit(EXIT_FAILURE);
                }
                if(child3==0){ // child process for command3
                    dup2(pipefd2[0], STDIN_FILENO); // replace stdin with the read end of pipe2
                    close(pipefd1[1]); // Close the write end of pipe1
                    close(pipefd1[0]); // Close the read end of pipe1
                    close(pipefd2[1]); // Close the write end of pipe2
                    close(pipefd2[0]); // Close the read end of pipe2
                    int return_status = new_execute_helper(command3, output_fd); // execute command3
                    if (return_status == -1)
                    {
                        perror("child3 execution failed error"); // error handling
                        exit(EXIT_FAILURE);
                    }
                }
                else
                {
                close(pipefd1[0]); // Close the read end of pipe1
                close(pipefd1[1]); // Close the write end of pipe1
                close(pipefd2[0]); // Close the read end of pipe2
                close(pipefd2[1]); // Close the write end of pipe2
                wait(NULL); // Wait for first child
                wait(NULL); // Wait for second child
                wait(NULL); // Wait for third child
                return 0;
            }
        }    
    }    
}
}

// command1 | command2 | command3 | command4
int execute_three_pipes(char *command1, char *command2, char *command3, char *command4, int output_fd) {
   int pipefd1[2]; // create three pipes
   int pipefd2[2];
   int pipefd3[2];
   if (pipe(pipefd1) == -1) { // Make sure the pipe was created successfully
        perror("pipe1 failed error");
        exit(EXIT_FAILURE);
    }
    if (pipe(pipefd2) == -1) { // Make sure the pipe was created successfully
        perror("pipe2 failed error");
        exit(EXIT_FAILURE);
    }
    if (pipe(pipefd3) == -1) { // Make sure the pipe was created successfully
        perror("pipe3 failed error");
        exit(EXIT_FAILURE);
    }
    pid_t child1=fork(); // fork a child process so not to interrupt the main process
    if(child1==-1){
        perror("fork1 failed error");
        exit(EXIT_FAILURE);
    }
    if(child1==0){ // child process for command1
        dup2(pipefd1[1], STDOUT_FILENO); // replace stdout with the write end of pipe1
        close(pipefd1[1]); // Close the write end of pipe1
        close(pipefd1[0]); // Close the read end of pipe1
        close(pipefd2[1]); // Close the read end of pipe2
        close(pipefd2[0]); // Close the write end of pipe2
        close(pipefd3[1]); // Close the read end of pipe3
        close(pipefd3[0]); // Close the write end of pipe3
        execute_helper(command1, output_fd); // execute command1
        
        perror("child1 execution failed error"); // error handling
        exit(EXIT_FAILURE);
    }
    else{
        pid_t child2=fork(); // fork another child process for command2
        if(child2==-1){ // fork error
            perror("fork2 failed error");
            exit(EXIT_FAILURE);
        }
        if(child2==0){ // child process for command2
            dup2(pipefd1[0], STDIN_FILENO); // replace stdin with the read end of pipe1
            dup2(pipefd2[1], STDOUT_FILENO); // replace stdout with the write end of pipe2
            close(pipefd1[1]); // Close the write end of pipe1
            close(pipefd1[0]); // Close the read end of pipe1
            close(pipefd2[1]); // Close the write end of pipe2
            close(pipefd2[0]); // Close the read end of pipe2
            close(pipefd3[1]); // Close the read end of pipe3
            close(pipefd3[0]); // Close the write end of pipe3
            execute_helper(command2, output_fd); // execute command2
            
            perror("child2 execution failed error");
            exit(EXIT_FAILURE);
        }
        else{
            pid_t child3=fork(); // fork another child process for command3
            if(child3==-1){ // fork error
                perror("fork3 failed error");
                exit(EXIT_FAILURE);
            }
            if(child3==0){ // child process for command3
                dup2(pipefd2[0], STDIN_FILENO); // replace stdin with the read end of pipe2
                dup2(pipefd3[1], STDOUT_FILENO); // replace stdout with the write end of pipe3
                close(pipefd1[1]); // Close the write end of pipe1
                close(pipefd1[0]); // Close the read end of pipe1
                close(pipefd2[1]); // Close the write end of pipe2
                close(pipefd2[0]); // Close the read end of pipe2
                close(pipefd3[1]); // Close the read end of pipe3
                close(pipefd3[0]); // Close the write end of pipe3
                execute_helper(command3, output_fd); // execute command3
                
                perror("child3 execution failed error"); // error handling
                exit(EXIT_FAILURE);
            }
            else{
                pid_t child4=fork(); // fork another child process for command4
                if(child4==-1){ // fork error
                    perror("fork4 failed error");
                    exit(EXIT_FAILURE);
                }
                if (child4==0)
                { // child process for command4
                    dup2(pipefd3[0], STDIN_FILENO); // replace stdin with the read end of pipe3
                    close(pipefd1[1]); // Close the write end of pipe1
                    close(pipefd1[0]); // Close the read end of pipe1
                    close(pipefd2[1]); // Close the write end of pipe2
                    close(pipefd2[0]); // Close the read end of pipe2
                    close(pipefd3[1]); // Close the read end of pipe3
                    close(pipefd3[0]); // Close the write end of pipe3
                    int return_status = new_execute_helper(command4, output_fd); // execute command3
                    if (return_status == -1)
                    {
                        perror("child4 execution failed error"); // error handling
                        exit(EXIT_FAILURE);
                    }
                }
                else
                {
                    close(pipefd1[0]); // Close the read end of pipe1
                    close(pipefd1[1]); // Close the write end of pipe1
                    close(pipefd2[0]); // Close the read end of pipe2
                    close(pipefd2[1]); // Close the write end of pipe2
                    close(pipefd3[0]); // Close the read end of pipe3
                    close(pipefd3[1]); // Close the write end of pipe3
                    wait(NULL); // Wait for first child
                    wait(NULL); // Wait for second child
                    wait(NULL); // Wait for third child
                    wait(NULL); // Wait for fourth child
                    return 0;
                }
            }
        }
    } 
}

// Execute the user's command [ovrall function]
int execute_command(char *input, int fd) 
{

    int pipe_count = 0;         // Number of pipes in the command
 
    // Remove the trailing newline character
    if (input[strlen(input) - 1] == '\n') {
        input[strlen(input) - 1] = '\0';
    }

    // Count the number of pipes
    for (int i = 0; i < strlen(input); i++) {
        if (input[i] == '|') {
            pipe_count++;
        }
    }

    // Create a copy of the input string because strtok will modify it
    char *buffer = strdup(input);
    if (!buffer) {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    // Create an array of commands, dealing with pipes
    char **commands =create_args(input, " | ");

    free(buffer); // Free the memory allocated for the buffer
    // This for loop is to check whether there're empty commands in the pipeline
    for (int i = 0; i < pipe_count + 1; i++) 
    {
        if (commands[i] == NULL) 
        {
            printf("Empty command!\n");
        }
    }
    if (pipe_count == 0) // if there is no pipe, then we just execute the single command
    {   
        execute_single_command(commands[0], fd);
    } 
    else if (pipe_count == 1) // if there is one pipe, then we execute the two commands
    {   
        
        execute_one_pipe(commands[0], commands[1], fd);
    } 
    else if (pipe_count == 2) // if there are two pipes, then we execute the three commands
    {
        execute_two_pipes(commands[0], commands[1], commands[2], fd);
    } 
    else if (pipe_count == 3) // if there are three pipes, then we execute the four commands
    {
        execute_three_pipes(commands[0], commands[1], commands[2], commands[3], fd);
    }
    free(commands); // Free the memory allocated for the commands
    

    return 0;
}




