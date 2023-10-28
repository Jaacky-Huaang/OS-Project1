all: shell_server shell_framework_client

shell_server: shell_server.o execute_command.o
	gcc -o shell_server shell_server.o execute_command.o
shell-server.o: shell_server.c execute_command.h
	gcc -c shell_server.c
execute_command.o: execute_command.c execute_command.h
	gcc -c execute_command.c
shell_framework_client: shell_framework_client.o execute_command.o
	gcc -o shell_framework_client shell_framework_client.o execute_command.o

shell_framework_client.o: shell_framework_client.c execute_command.h
	gcc -c shell_framework_client.c


clean:
	rm -f execute_command.o shell_framework_client.o shell_framework_client
