#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) 
{
    // check the number of arguments
    if (argc != 2) {
        printf("Usage: %s <sleep_seconds>\n", argv[0]);
        return 1;
    }
    int sleep_seconds = atoi(argv[1]);

    // print every second
    for (int i = 0; i < sleep_seconds; i++) 
    {
        printf("Demo %d/%d\n", i, sleep_seconds);
        sleep(1);
    }

    return 0;
}
