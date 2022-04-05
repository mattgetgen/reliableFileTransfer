# Reliable File Transfer
#### By Matthew Getgen
---

## Project Description
This project's goal is to create a reliable file transfer protocol over UDP packets.
Both the client and server handle reliable communication with a specific communication
protocol and packet standard.

## File Description
old-client.c:
old-server.c:
	Program files that use the communication protocol and packet standard created for the
	College Project I made this for. It's packet standard is outdated, and it has some
	bugs, but for the time it was created, worked for the assignment flawlessly.

client.c:
server.c:
	Program files that use the same communication protocol as last time, but with a 
	revised packet protocol. This packet layout and related funcitons are in the 
	`packet.c` and `packet.h` files. While these are much cleaner programs and use
	a better packet header standard, they are still flawed at a communication
	algorithm standpoint. The next iteration of this project will handle this.

packet.c:
packet.h:
	These files are for defining the packet standard used in both the client and server 
	files. Rather than have the same code in both, it is written in one location for
	both files to use.

Makefile:
	Compliles and runs the client and server programs, as well as their older variants.

## How to Install and Run the Project
To install this project, simply clone this repo. Then, run make all to compile all of the
files. To run, make sure to change the remote and local file directory arguments to pass to
the client.

Server requires arguments: ./server <Server Port>

Client requires arguments: ./client <Server IP> <Server Port> <Remote Path> <Local Path>


