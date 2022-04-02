#
# Matthew Getgen's makefile for RFT
#
CC = gcc
CFLAGS = -Wall -Wextra -Werror -g

new_src  = packet.c client.c server.c
new_obj  = packet.o client.o server.o
new_exec = client server

old_src  = old-client.c old-server.c
old_exec = old-client old-server

all: new old

new: $(new_obj)
	$(CC) $(CFLAGS) -o client packet.o client.o
	$(CC) $(CFLAGS) -o server packet.o server.o

$(new_obj): $(new_src)
	$(CC) $(CFLAGS) -c $(^)

runc: client
	./client 127.0.0.1 8080 ./remote ./local

runs: server
	./server 8080



old: $(old_exec)

$(old_exec): $(old_src)
	$(CC) $(CFLAGS) -o $(@) $(@).c

runoc: old-client
	./old-client 127.0.0.1 8080 ./remote ./local

runos: old-server
	./old-server 8080


#
# Clean the src directory
#
clean: $(new_obj)
	rm -f $(new_exec) $(old_exec) $(^)

