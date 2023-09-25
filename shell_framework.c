#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
    char input[256];
    printf("MyShell session started...\n");
    while (1) {
        // ask for a prompt
        printf("Enter Shell Command:\n>");
        scanf(" %[^\n]", input);
        // Exit the shell if the user enters "exit" or "quit"
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            printf("Exiting MyShell...\n");
            break;
        }

        // Execute the user's command
        execute_command(input);
    }

    return 0;
}






