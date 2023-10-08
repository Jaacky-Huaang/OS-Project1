#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_INPUT_SIZE 256
#define MAX_ARGS 64
#define MAX_ARG_LENGTH 32 // 每个参数的最大长度
#define MAX_CMDS 8



//utility functions
//_______________________________________________________________________________________
char **create_args(const char *command, const char *seperator) {
    char **args = (char **)malloc(MAX_ARGS * sizeof(char *));
    if (!args) {
        perror("Failed to allocate memory for args");
        exit(EXIT_FAILURE);
    }

    char *command_copy = strdup(command);
    if (!command_copy) {
        perror("Failed to copy command");
        free(args);
        exit(EXIT_FAILURE);
    }

    int arg_count = 0;
    char *component = strtok(command_copy, seperator);
    while (component != NULL && arg_count < MAX_ARGS - 1) {
        args[arg_count] = component;
        arg_count++;
        component = strtok(NULL, seperator);
    }
    args[arg_count] = NULL;


    free(command_copy);
    return args;
}

//_______________________________________________________________________________________

int input_redirect(char *command)
{   
    //handle the input redirection by getting the input file name
    //sample: cat < file...
    char *input_file = NULL;
    char *command_copy = strdup(command);
    char *input_redirect = strstr(command_copy, "<");
    //redirect is pointer pointing to the first occurence of "<"
    if (input_redirect) //if there is indeed "<"
    {
        *input_redirect = '\0'; //replace the first occurence of "<" with '\0'
        input_file = strtok(input_redirect + 1, " "); //get the file name
        input_file += strspn(input_file, " \t"); //skip the white space
    }
    
}

int execute_helper(char *command) 
{

    if (strchr(command, '<') )
    {
        return input_redirect(command);
    }

    //这里的条件是互斥的，不知道你能不能写成可以一起执行的

    else if (strchr(command, '>') )
    {
        return output_redirect(command);
    }

    else if (strchr(command, '2>') )
    {
        return error_redirect(command);
    }

    else
    { 

        char **args = create_args(command, " ");
        for (int i = 0; args[i] != NULL; i++) {
            printf("args[%d] = %s\n", i, args[i]);
        }

        if (execvp(args[0], args) == -1) {
            perror("Error executing command");
            exit(EXIT_FAILURE); // Only the child exits
        }

        free(args);
    }
    return 1; // Return 1 to continue the shell
}

int execute_single_command(char *command) 
{

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { // Child process
        execute_helper(command);
    } else { // Parent process
        wait(NULL);
    }
    return 0;
}

//command1 | command2
int execute_one_pipe(char *command1, char *command2) {
    
    printf("Executing one-pipe command: %s | %s\n", command1, command2);
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if(pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0) { // Child process for command1
        close(pipefd[0]);//close the read end of the pipe
        dup2(pipefd[1], STDOUT_FILENO);
        execute_helper(command1);//execute command1
    }

    close(pipefd[1]);

    pid_t pid2 = fork();

    if (pid2 == 0) { // Child process for command2
        close(pipefd[1]);//close the write end of the pipe
        dup2(pipefd[0], STDIN_FILENO);//replace stdin with the read end of the pipe
        execute_helper(command2);
    }

    close(pipefd[0]);//close the read end of the pipe

    wait(NULL); // Wait for first child
    wait(NULL); // Wait for second child

    return 0;
}


int execute_command(char *input) {

    char *commands[MAX_CMDS];  // Assuming a maximum of 8 commands in a pipeline
    int pipe_count = 0;         // Number of pipes in the command
 
    printf("Executing: %s\n", input);

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


    // Split the command based on pipes
    /*
    char *token = strtok(input, " | ");
    
    */
    char *buffer = strdup(input);
    if (!buffer) {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    int index = 0;
    char *cmd_start = buffer;
    int in_command = 0;

    for (char *ptr = buffer; *ptr != '\0'; ptr++) {
        if (*ptr == '|') {
            if (in_command) {
                *ptr = '\0';
                commands[index++] = strdup(cmd_start);
                in_command = 0;
            }
            while (*(ptr + 1) == ' ') ptr++;  // skip spaces after |
        } else if (!in_command) {
            cmd_start = ptr;
            in_command = 1;
        }
    }

    if (in_command) {
        commands[index++] = strdup(cmd_start);
    }

    commands[index] = NULL;  // Terminate the list

    free(buffer);


    // Print results
    printf("Number of pipes: %d\n", pipe_count);
    printf("Commands:\n");
    for (int i = 0; i <= pipe_count; i++) {
        printf("%d: %s\n", i + 1, commands[i]);
    }

    
    if (pipe_count == 0) 
    {   
        execute_single_command(commands[0]);
    } 
    
    else if (pipe_count == 1) 
    {   
        
        execute_one_pipe(commands[0], commands[1]);
    } 
    /*
    else if (pipe_count == 2) 
    {
        execute_two_commands(commands[0], commands[1], commands[2]);
    } 
    else if (pipe_count == 3)
    {
        execute_three_commands(commands[0], commands[1], commands[2], commands[3]);
    }
    */

    return 0;
}

/*

    char *output_file = NULL;
    char *error_file = NULL;

    //do the same for ">" to get the output file name
    command_copy = strdup(command);
    char *output_redirect = strstr(command_copy, ">");
    if (output_redirect) {
        *output_redirect = '\0';
        output_file = strtok(output_redirect + 1, " ");
        output_file += strspn(output_file, " \t");
    }


    //do the same for "2>" to get the error file name
    command_copy = strdup(command);
    char *error_redirect = strstr(command_copy, "2>");
    if (error_redirect) {
        *error_redirect = '\0';
        error_file = strtok(error_redirect + 2, " ");
        error_file += strspn(error_file, " \t");
    }

    // Redirecting input if specified
    if (input_file != NULL) {
        printf("input file: %s\n", input_file);
        int in_fd = open(input_file, O_RDONLY);
        if (in_fd == -1) {
            perror("Error opening input file");
            exit(EXIT_FAILURE);
        }
        dup2(in_fd, STDIN_FILENO);
        close(in_fd);
    }

    // Redirecting output if specified
    if (output_file != NULL) {
        int out = open(output_file, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
        if (out == -1) {
            perror("Error opening output file");
            exit(EXIT_FAILURE);
        }
        dup2(out, STDOUT_FILENO);
        close(out);
    }
*/
