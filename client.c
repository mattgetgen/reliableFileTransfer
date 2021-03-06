/**
 * @file client.c
 * @author Matthew Getgen (matt_getgen@taylor.edu)
 * @brief reliable file transfer client
 * @version 0.1
 * @date 2022-03-14
 */

#include "packet.h"

#define IS_SERVER 0

/*
 * This Program makes reliable file transfers using a protocol that I designed.
 * It may not be perfect, but I plan on testing it and optimizing it.
 * 
 * This program creates a packet that is ready to be sent over a network with a
 * specific header and values.
 */

// struct for storing connection and message data
typedef struct connection {
    struct addrinfo *p;
    int socket_desc;
} connection;

// send the packet and information. Can print the packet being sent, because it has already been parsed
int send_data(connection *connect, Packet *packet, int line) {
    int rv =  (int)sendto(connect->socket_desc, packet, get_packet_size(packet), 0, connect->p->ai_addr, connect->p->ai_addrlen);
    if (rv == -1) print_error(strerror(errno), line);
    else                    print_packet(packet, 1, IS_SERVER);
    return rv;
}

// received the packet and information. Cannot print the packet that was received, because it has not already been parsed
int recv_data(connection *connect, Packet *packet) {
    return (int)recvfrom(connect->socket_desc, packet, sizeof(Packet), 0, connect->p->ai_addr, &connect->p->ai_addrlen);
}

int send_acknowledgement(connection *connect, Packet *ack_packet, u_int ack_num) {
    // for acknowledgement:       2 is ACK packet
    set_packet_header(ack_packet, 2, 0, ack_num, 100, sizeof(packet_header));
    return send_data(connect, ack_packet, __LINE__);
}

int wait_for_acknowledgement(connection *connect, Packet *send_packet, Packet *recv_packet, int i) {
    int rv;
    u_int seq_num, ack_num;
    seq_num = send_packet->header.seq_num;

    if (i < MAX_RETRIES) {  // if tried less than 8 times, wait to receive data

        rv = recv_data(connect, recv_packet);
        if (rv == -1) {   // if havent received data

            rv = send_data(connect, send_packet, __LINE__); // resend data
            if (rv == -1) return rv;

            i = wait_for_acknowledgement(connect, send_packet, recv_packet, i+1);   // wait yet again
            if (i == -1) return i;

        } else {    // else have received data

            print_packet(recv_packet, 0, IS_SERVER);
            ack_num = recv_packet->header.seq_num;
            if (!is_packet_acknowledgement(recv_packet) || ack_num != seq_num) {   // if not the correct response

                rv = send_data(connect, send_packet, __LINE__); // resend data
                if (rv == -1) return rv;

                i = wait_for_acknowledgement(connect, send_packet, recv_packet, i+1);   // wait yet again
                if (i == -1) return i;
            }
        }
        // is correct data, return
        return rv;

    } else {    // more than maximum number of tries, return error
        print_error("Connection Closed.", __LINE__);
        return -1;
    }
}

int handle_connection(connection *connect, char *remote_file, char *local_file) {
    FILE *file;
    int rv, i = 0;
    u_int seq_num = 1, temp;
    u_short recv_size;

    Packet send_packet = init_packet();
    Packet recv_packet = init_packet();

    // for inital request           1 is SEQ packet
    set_packet_header(&send_packet, 1, 0, seq_num, 100, strlen(remote_file));
    memcpy(send_packet.buff, remote_file, strlen(remote_file));
    
    // send request header
    rv = send_data(connect, &send_packet, __LINE__);
    if (rv == -1) return rv;
    
    // wait for acknowledgement
    rv = wait_for_acknowledgement(connect, &send_packet, &recv_packet, 1);
    if (rv == -1) {
        return rv;
    }
    
    // open local file
    if ((file = fopen(local_file, "w")) == NULL) {
        print_error(strerror(errno), __LINE__);
        return -1;
    }

    do {    // while is not a finale packet, and tried less than 8 times
        rv = recv_data(connect, &recv_packet);
        // didn't receive data
        if (rv == -1) {
            printf(".");
            fflush(stdout);
            i++;
        } else {    // received data
            i = 0;
            
            print_packet(&recv_packet, 0, IS_SERVER);
            temp = recv_packet.header.seq_num;

            // if is a sequence packet
            if (is_packet_sequence(&recv_packet)) {
                
                // if correct next packet
                if (temp == seq_num+1) {
                    // write data to file, move to next packet
                    seq_num = temp;
                    recv_size = recv_packet.header.data_size;
                    fwrite(recv_packet.buff, recv_size, 1, file);
                    memset(recv_packet.buff, 0, MAX_BUFFER_SIZE);
                }

                // send acknowledgement, regardless if its next packet or previous packet
                rv = send_acknowledgement(connect, &send_packet, seq_num);
                if (rv == -1) return rv;

            // not a sequence packet. Should either be an error or a finale
            } else {
                fclose(file);

                // send acknowledgement
                rv = send_acknowledgement(connect, &send_packet, temp);
                if (rv == -1) return rv;

                // if it is an error packet
                if (is_packet_error(&recv_packet)) {
                    print_error_msg(&recv_packet, __LINE__);
                    return -1;

                // else if it is a finale packet
                } else if (is_packet_finale(&recv_packet)) {
                    printf("\nFile Transfer Complete!");
                }

            }
        }
    } while (!is_packet_finale(&recv_packet) && i < MAX_RETRIES);

    if (i >= MAX_RETRIES) {
        print_error("Connection Closed.", __LINE__);
        return -1;
    }
    
    return 0;
}

void manage_file_path(char *file_buff, char *file_path, char *file_name) {
    strncat(file_buff, file_path, strlen(file_path));
    strncat(file_buff, file_name, strlen(file_name));
    return;
}

int handle_file_names(char *remote_buff, char *local_buff, char *remote_path, char *local_path) {
    char file_name[MAX_BUFFER_SIZE] = "/";
    char *file_name_ptr = &file_name[1];
    size_t remote_size = strlen(remote_path), local_size = strlen(local_path), file_size;

    if (remote_size >= MAX_BUFFER_SIZE || local_size >= MAX_BUFFER_SIZE) {
        print_error("Remote or Local Path are too big!", __LINE__);
        return -1;
    }

    printf("\nEnter a file name: ");
    scanf("%s", file_name_ptr);
    file_size = strlen(file_name);

    if ( (file_size+remote_size) >= MAX_BUFFER_SIZE || (file_size+local_size) >= MAX_BUFFER_SIZE) {
        print_error("File name is too big!", __LINE__);
        return -1;
    }
    manage_file_path(remote_buff, remote_path, file_name);
    manage_file_path(local_buff, local_path, file_name);

    return 0;
}

int main(int argc, char *argv[]) {
    int rv;

	char *SERVER_IP, *SERVER_PORT, *REMOTE_PATH, *LOCAL_PATH;
    char remote_file[MAX_BUFFER_SIZE];
    char local_file[MAX_BUFFER_SIZE];
    memset(remote_file, 0, MAX_BUFFER_SIZE);
    memset(local_file, 0, MAX_BUFFER_SIZE);

	int socket_desc;
	struct addrinfo hints, *servInfo, *p;
    struct timeval tv;

    connection connect;
	time_t start, end;

	// command line arguments
	if (argc != 5) {
        printf("\nArguments expected: <Server IP> <Server Port> <Remote Path> <Local Path>");
        return -1;
    }
    SERVER_IP = argv[1];
    SERVER_PORT = argv[2];
    REMOTE_PATH = argv[3];
    LOCAL_PATH = argv[4];
    printf("server IP: %s\nserver port: %s\nremote path: %s\nlocal path: %s\n", SERVER_IP, SERVER_PORT, REMOTE_PATH, LOCAL_PATH);

	memset(&hints, 0, sizeof(hints));// set all data in struct to 0
	hints.ai_family = AF_INET;          // IPv4
	hints.ai_socktype = SOCK_DGRAM;     // UDP

	// set timer
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    rv = getaddrinfo(SERVER_IP, SERVER_PORT, &hints, &servInfo);
    if (rv != 0) {
        print_error("getaddrinfo failed.", __LINE__);
        return rv;
    }

    for (p = servInfo; p != NULL; p = p->ai_next) {
        // socket(): creates a new socket, no address was assigned yet
        socket_desc = socket(
                p->ai_family,    // IPv4
                p->ai_socktype,  // Streaming Protocol
                p->ai_protocol   // UDP
        );
        if (socket_desc == -1) {
            print_error(strerror(errno), __LINE__);
            continue;
        }
        break;
    }

    rv = setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *) &tv, sizeof(struct timeval));
    if (rv == -1 && p == NULL) {
        print_error(strerror(errno), __LINE__);
        return rv;
    }

    rv = handle_file_names(remote_file, local_file, REMOTE_PATH, LOCAL_PATH);
    if (rv == -1) {
        return rv;
    }

    freeaddrinfo(servInfo);

    // set connection data
    connect.p = p;
    connect.socket_desc = socket_desc;

    start = time(NULL);
    rv = handle_connection(&connect, remote_file, local_file);
    end = time(NULL);
    printf("\nTime elapsed: %ld\n", end-start);
    close(socket_desc);

    return rv;
}

