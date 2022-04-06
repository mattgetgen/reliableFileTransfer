/**
 * @file server.c
 * @author Matthew Getgen (matt_getgen@taylor.edu)
 * @brief reliable file transfer server
 * @version 0.1
 * @date 2022-03-14
 */

#include "packet.h"

#define IS_SERVER 1

/*
 * This Program makes reliable file transfers using a protocol that I designed.
 * It may not be perfect, but I plan on testing it and optimizing it.
 * 
 * This program creates a packet that is ready to be sent over a network with a
 * specific header and values.
 */

typedef struct connection {
    struct sockaddr_storage *remote_addr;
    socklen_t addr_len;
    int socket_desc;
} connection;

int send_data(connection *connect, Packet *packet, int line) {
    int rv = (int)sendto(connect->socket_desc, packet, get_packet_size(packet), 0, (struct sockaddr *)connect->remote_addr, connect->addr_len);
    if (rv == -1) print_error(strerror(errno), line);
    else                    print_packet(packet, 1, IS_SERVER);
    return rv;
}

int recv_data(connection *connect, Packet *packet) {
    return (int)recvfrom(connect->socket_desc, packet, sizeof(Packet), 0, (struct sockaddr *)connect->remote_addr, &connect->addr_len);
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

int send_finale_packet(connection *connect, Packet *send_packet, Packet *recv_packet, u_int seq_num) {
    int rv;
    // for finale packet:          3 is FIN packet
    set_packet_header(send_packet, 3, 0, seq_num, 100, sizeof(packet_header));
    rv = send_data(connect, send_packet, __LINE__);
    if (rv == -1) return rv;
    rv = wait_for_acknowledgement(connect, send_packet, recv_packet, 1);
    return rv;
}

// any result from this is to quit, so always return -1
int send_error_packet(connection *connect, Packet *send_packet, Packet *recv_packet, u_int error_num) {
    // for error packet:           0 is ERR packet
    set_packet_header(send_packet, 0, error_num, 0, 100, sizeof(packet_header));
    send_data(connect, send_packet, __LINE__);
    wait_for_acknowledgement(connect, send_packet, recv_packet, 1);
    return -1;
}

int send_file(connection *connect, Packet *send_packet, Packet *recv_packet) {
    int rv;
    FILE *file;
    u_char c;
    //long file_size;
    u_int buffNum = 0, seq_num = send_packet->header.seq_num;

    if (access((char *)recv_packet->buff, F_OK) == 0) {
        if ((file = fopen((char *)recv_packet->buff, "rb")) == NULL) {
            print_error(strerror(errno), __LINE__);                  // 2 is File Not Found
            return send_error_packet(connect, send_packet, recv_packet, 2);
        }
/*
        // get file size
        fseek(file, 0, SEEK_END);
        file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
*/

        while (fread(&c, 1, 1, file) == 1) {

            if (buffNum < MAX_BUFFER_SIZE-1) {
                strncat((char *)send_packet->buff, (char *)&c, 1);
                buffNum++;
            } else {
                strncat((char *)send_packet->buff, (char *)&c, 1);

                seq_num++;
                // for sequence packet:        1 is SEQ packet
                set_packet_header(send_packet, 1, 0, seq_num, 100, strlen((char *)send_packet->buff));

                rv = send_data(connect, send_packet, __LINE__);
                if (rv == -1) return rv;

                rv = wait_for_acknowledgement(connect, send_packet, recv_packet, 1);
                if (rv == -1) return rv;

                buffNum = 0;
                memset(send_packet->buff, 0, MAX_BUFFER_SIZE);
            }
        }
        fclose(file);
        
        seq_num++;
        // for sequence packet:        1 is SEQ packet
        set_packet_header(send_packet, 1, 0, seq_num, 100, strlen((char *)send_packet->buff));

        rv = send_data(connect, send_packet, __LINE__);
        if (rv == -1) return rv;

        rv = wait_for_acknowledgement(connect, send_packet, recv_packet, 1);
        if (rv == -1) return rv;

        memset(send_packet->buff, 0, MAX_BUFFER_SIZE);
        rv = send_finale_packet(connect, send_packet, recv_packet, seq_num);
        if (rv == -1) return rv;

    } else {
        //file doesn't exist
        print_error(strerror(errno), __LINE__);                  // 2 is File Not Found
        return send_error_packet(connect, send_packet, recv_packet, 2);
    }

    return 0;
}

int handle_connection(int socket_desc, time_t *start) {
    int rv;
    u_int seq_num;

    struct sockaddr_storage remote_addr;
    socklen_t addr_len = sizeof(remote_addr);

    connection connect;

    Packet send_packet = init_packet();
    Packet recv_packet = init_packet();

    // set connection data
    connect.remote_addr = &remote_addr;
    connect.addr_len = addr_len;
    connect.socket_desc = socket_desc;
    
    while (1) {
        rv = recv_data(&connect, &recv_packet);
        if (rv == -1) {
            printf(".");
            fflush(stdout);
            continue;
        } else {
            *start = time(NULL);
            print_packet(&recv_packet, 0, IS_SERVER);
            break;
        }
    }

    seq_num = recv_packet.header.seq_num;
    rv = send_acknowledgement(&connect, &send_packet, seq_num);
    if (rv == -1) return rv;

    if (!is_packet_sequence(&recv_packet) || seq_num != 1) {
        return send_error_packet(&connect, &send_packet, &recv_packet, 1);
    } else {                                                        // 1 is Bad Request
        rv = send_file(&connect, &send_packet, &recv_packet);
        return rv;
    }
}

int main(int argc, char *argv[]) {
    int rv = 0;
    char * MY_PORT;

    int socket_desc;
    struct addrinfo hints, *servInfo, *p;
    struct timeval tv;

    time_t start, end;

    // command line arguments
	if (argc != 2) {
        printf("\nArguments expected: <Server Port>");
        return -1;
    }
    MY_PORT = argv[1];
    printf("server port: %s\n", MY_PORT);

    memset(&hints, 0, sizeof(hints)); // set all data in struct to 0
    hints.ai_family = AF_INET;           // IPv4
    hints.ai_socktype = SOCK_DGRAM;      // UDP
    hints.ai_flags = AI_PASSIVE;         // Listen

    // set timer
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    rv = getaddrinfo(NULL, MY_PORT, &hints, &servInfo);
    if (rv != 0) {
        print_error(strerror(errno), __LINE__);
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
        rv = bind(socket_desc, p->ai_addr, p->ai_addrlen);
        if (rv == -1) {
            close(socket_desc);
            print_error(strerror(errno), __LINE__);
            continue;
        }
        break;
    }

    rv = setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));
    if (rv == -1 && p == NULL) {
        print_error(strerror(errno), __LINE__);
        return rv;
    }

    freeaddrinfo(servInfo);

    rv = handle_connection(socket_desc, &start);
    end = time(NULL);
    printf("\nTime elapsed: %ld\n", end-start);
    close(socket_desc);

    return rv;
}

