#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

#define MAX_INPUT_SIZE 256
#define MAX_ARGS 64
#define MAX_ARG_LENGTH 32 // 每个参数的最大长度
#define MAX_CMDS 8


//a utility function to create arguments for the command
char **create_args(const char *command, const char *separator) {
    char **args = (char **)malloc(MAX_ARGS * sizeof(char *));
    if (!args) {
        perror("Failed to allocate memory for args");
        exit(EXIT_FAILURE);
    }

    char *command_copy = strdup(command);//make a copy of the command because strtok will modify it
    if (!command_copy) {
        perror("Failed to copy command");
        free(args);
        exit(EXIT_FAILURE);
    }

    int arg_count = 0;
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

int input_redirect(char *command)
{   
    char **args = create_args(command, " < ");
    char *sub_command = args[0];
    char *input_file = args[1]; 

    int fd = open(input_file, O_RDONLY);  // open the input file

    if (fd < 0) {
        perror("Failed to open input file");
        exit(1);
    }

    char **sub_args = create_args(sub_command, " ");//create arguments for the command
    printf("Executing command: %s\n", sub_command);
    dup2(fd, STDIN_FILENO);  // duplicate file descriptor to stdin
    close(fd);  // close the original file descriptor
    execvp(sub_args[0], sub_args);  // execute the command
    perror("Exec failed");  // execvp will only return if there's an error
    exit(1);
    return 0;
}


int output_redirect(char *command)
{   
    //redirecting output
    char **args = create_args(command, " > ");
    char *sub_command = args[0];
    char *output_file = args[1];

    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);  // open the input file, googled convention
    if (fd < 0) {
        perror("Failed to open input file");
        exit(1);
    }

    char **sub_args = create_args(sub_command, " ");

    dup2(fd, STDOUT_FILENO);  // duplicate file descriptor to stdout
    close(fd);  // close the original file descriptor
    printf("Executing command: %s\n", sub_command);
    execvp(sub_args[0], sub_args);  // execute the command
    perror("Exec failed");  // execvp will only return if there's an error
    exit(1);
    return 0;
    
}

int error_redirect(char *command) {   
    char **args = create_args(command, " 2> ");
    char *sub_command = args[0];
    char *error_file = args[1];

    int fd = open(error_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);  // open the input file, googled convention
    if (fd < 0) {
        perror("Failed to open error file");
        exit(1);
    }

    char **sub_args = create_args(sub_command, " ");
    printf("Executing command: %s\n", sub_command);
    dup2(fd, STDERR_FILENO);  // Redirect stderr to the error file
    close(fd);  
    close(STDOUT_FILENO);  // Close stdout
    execvp(sub_args[0], sub_args);  // execute the command
    perror("Exec failed");  
    exit(1);
    return 0;
}

int execute_helper(char *command) 
{

    if (strchr(command, '<') )//if the command contains <, then it is an input redirection
    {
        return input_redirect(command);
    }

    //这里的条件是互斥的，不知道你能不能写成可以一起执行的
    else if (strstr(command, "2>") )
    {
        return error_redirect(command); // Redirect stderr to the error file
    }

    else if (strchr(command, '>')  )
    {
        return output_redirect(command);//if the command contains >, then it is an output redirection
    }

    else
    { 
        char **args = create_args(command, " ");//if we do not have >, <, 2>, then we just create arguments for the command
        printf("Executing command: %s\n", command);
        if (execvp(args[0], args) == -1) {
            perror("Error executing command");
            exit(EXIT_FAILURE); // Only the child exits
        }

        free(args);
    }
    return 1; // Return 1 to continue the shell
}

//command
//execute a single command without any pipes or redirection
int execute_single_command(char *command) 
{

    pid_t pid = fork();//fork a child process so not to interrupt the main process
    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } 
    if (pid == 0) { // Child process
        execute_helper(command);
    } else { // Parent process
        wait(NULL);
    }
    return 0;
}

//command1 | command2
int execute_one_pipe(char *command1, char *command2) {
    
    int pipefd[2];//create a pipe

    if (pipe(pipefd) == -1) {
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
        execute_helper(command1);//execute command1
    }

    close(pipefd[1]);

    pid_t pid2 = fork();

    if (pid2 == 0) { // Child process for command2
        close(pipefd[1]);//close the write end of the pipe
        dup2(pipefd[0], STDIN_FILENO);//replace stdin with the read end of the pipe
        execute_helper(command2);//execute command2
        
    }

    close(pipefd[0]);//close the read end of the pipe

    wait(NULL); // Wait for first child
    wait(NULL); // Wait for second child

    return 0;
}

int execute_two_pipes(char *command1, char *command2, char *command3) {
   printf("Executing two-pipes command: %s | %s | %s\n", command1, command2,command3);
   int pipefd1[2];
   int pipefd2[2];
   if (pipe(pipefd1) == -1) {
        perror("pipe1 failed error");
        exit(EXIT_FAILURE);
    }
    if (pipe(pipefd2) == -1) {
        perror("pipe2 failed error");
        exit(EXIT_FAILURE);
    }
    pid_t child1=fork();
    if(child1==-1){
        perror("fork1 failed error");
        exit(EXIT_FAILURE);
    }
    else{
        if(child1==0){
        dup2(pipefd1[1], STDOUT_FILENO);
        close(pipefd1[1]);
        close(pipefd1[0]);
        close(pipefd2[0]); // Close the read end of pipe2
        close(pipefd2[1]); 
        execute_helper(command1);
        perror("child1 execution failed error");
        exit(EXIT_FAILURE);
        }
        else{
            pid_t child2=fork();
            if(child2==-1){
                perror("fork2 failed error");
                exit(EXIT_FAILURE);
            }
            if(child2==0){
            dup2(pipefd1[0], STDIN_FILENO);
            dup2(pipefd2[1], STDOUT_FILENO);
            close(pipefd1[1]);
            close(pipefd2[0]);
            close(pipefd2[1]);
            close(pipefd1[0]);
            execute_helper(command2);
            perror("child2 execution failed error");
            exit(EXIT_FAILURE);
            }
            else{
                pid_t child3=fork();
                if(child3==-1){
                    perror("fork3 failed error");
                    exit(EXIT_FAILURE);
                }
                if(child3==0){
                    dup2(pipefd2[0], STDIN_FILENO);
                    close(pipefd1[1]);
                    close(pipefd1[0]);
                    close(pipefd2[1]);
                    close(pipefd2[0]);
                    execute_helper(command3);
                    perror("child3 execution failed error");
                    exit(EXIT_FAILURE);
                }
                else
                {
                close(pipefd1[0]);
                close(pipefd1[1]);
                close(pipefd2[0]);
                close(pipefd2[1]);
                wait(NULL);
                wait(NULL);
                wait(NULL);
                return 0;
            }
        }    
    }    
}
}


int execute_three_pipes(char *command1, char *command2, char *command3, char *command4) {
   printf("Executing three-pipes command: %s | %s | %s | %s\n", command1, command2,command3,command4);
   int pipefd1[2];
   int pipefd2[2];
   int pipefd3[2];
   if (pipe(pipefd1) == -1) {
        perror("pipe1 failed error");
        exit(EXIT_FAILURE);
    }
    if (pipe(pipefd2) == -1) {
        perror("pipe2 failed error");
        exit(EXIT_FAILURE);
    }
    if (pipe(pipefd3) == -1) {
        perror("pipe3 failed error");
        exit(EXIT_FAILURE);
    }
    pid_t child1=fork();
    if(child1==-1){
        perror("fork1 failed error");
        exit(EXIT_FAILURE);
    }
    if(child1==0){
        dup2(pipefd1[1], STDOUT_FILENO);
        close(pipefd1[1]);
        close(pipefd1[0]);
        close(pipefd2[1]); // Close the read end of pipe2
        close(pipefd2[0]);
        close(pipefd3[1]);
        close(pipefd3[0]);
        execute_helper(command1);
        
        perror("child1 execution failed error");
        exit(EXIT_FAILURE);
    }
    else{
        pid_t child2=fork();
        if(child2==-1){
            perror("fork2 failed error");
            exit(EXIT_FAILURE);
        }
        if(child2==0){
            dup2(pipefd1[0], STDIN_FILENO);
            dup2(pipefd2[1], STDOUT_FILENO);
            close(pipefd1[1]);
            close(pipefd1[0]);
            close(pipefd2[1]);
            close(pipefd2[0]);
            close(pipefd3[1]);
            close(pipefd3[0]);
            execute_helper(command2);
            
            perror("child2 execution failed error");
            exit(EXIT_FAILURE);
        }
        else{
            pid_t child3=fork();
            if(child3==-1){
                perror("fork3 failed error");
                exit(EXIT_FAILURE);
            }
            if(child3==0){
                dup2(pipefd2[0], STDIN_FILENO);
                dup2(pipefd3[1], STDOUT_FILENO);
                close(pipefd1[1]);
                close(pipefd1[0]);
                close(pipefd2[1]);
                close(pipefd2[0]);
                close(pipefd3[1]);
                close(pipefd3[0]);
                execute_helper(command3);
                
                perror("child3 execution failed error");
                exit(EXIT_FAILURE);
            }
            else{
                pid_t child4=fork();
                if(child4==-1){
                    perror("fork4 failed error");
                    exit(EXIT_FAILURE);
                }
                if (child4==0)
                {
                    dup2(pipefd3[0], STDIN_FILENO);
                    close(pipefd1[1]);
                    close(pipefd1[0]);
                    close(pipefd2[1]);
                    close(pipefd2[0]);
                    close(pipefd3[1]);
                    close(pipefd3[0]);
                    execute_helper(command4);
                    
                    perror("child4 execution failed error");
                    exit(EXIT_FAILURE);
                }
                else
                {
                    close(pipefd1[0]);
                    close(pipefd1[1]);
                    close(pipefd2[0]);
                    close(pipefd2[1]);
                    close(pipefd3[0]);
                    close(pipefd3[1]);
                    wait(NULL);
                    wait(NULL);
                    wait(NULL);
                    wait(NULL);
                    return 0;
                }
            }
        }
    } 
    
}




int execute_command(char *input) {

    //char *commands[MAX_CMDS];  // Assuming a maximum of 8 commands in a pipeline
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


    char **commands =create_args(input, " | ");

    free(buffer);
    printf("pipe count: %d\n", pipe_count);
    printf("commands: %s, %s, %s, %s\n", commands[0],commands[1],commands[2],commands[3]);
    
    if (pipe_count == 0) 
    {   
        execute_single_command(commands[0]);
    } 
    
    else if (pipe_count == 1) 
    {   
        
        execute_one_pipe(commands[0], commands[1]);
    } 
    else if (pipe_count == 2) 
    {
        execute_two_pipes(commands[0], commands[1], commands[2]);
    } 
    else if (pipe_count == 3)
    {
        execute_three_pipes(commands[0], commands[1], commands[2], commands[3]);
    }
    free(commands);

    return 0;
}




