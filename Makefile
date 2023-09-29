all: shell_framework

shell_framework: shell_framework.o execute_command.o
	gcc -o shell_framework shell_framework.o execute_command.o

shell_framework.o: shell_framework.c execute_command.h
	gcc -c shell_framework.c

execute_command_single.o: execute_command.c execute_command.h
	gcc -c execute_command.c

clean:
	rm -f execute_command.o shell_framework.o shell_framework
