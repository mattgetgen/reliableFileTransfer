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

/*
 *  send_acknowledgement():
 *      make ACK packet with ACK num from SEQ num;
 *      send packet to client;
 *
 *  wait_for_acknowledgement(i):
 *      if tried less than 8 times:
 *          wait to receive packet;
 *          if haven't receive acknowledgement within 2 seconds:
 *              resend data;
 *              wait_for_acknowledgement(i+1);
 *          else:
 *              if incorrect response:
 *                  resend data;
 *                  wait_for_acknowledgement(i+1);
 *              else:
 *                  return;
 *
 *  send_finale_packet():
 *      make FIN packet with same SEQ num as last;
 *      send_data();
 *      wait_for_acknowledgement(1);
 *
 *  send_error_packet():
 *      make ERR packet with ERR num;
 *      send_data();
 *      wait_for_acknowledgement(1);
 * 
 *  send_file():
 *      if you can open file:
 *          while not at EOF, read from file:
 *              if buffer is not full:
 *                  write data into buffer;
 *              else:
 *                  send data;
 *                  wait_for_acknowledgement(1);
 *          at end of while; (at this point it has written to buff without sending it)
 *          send buff;
 *          wait_for_acknowledgement(1);
 *      else:
 *          send ERR to client saying "file not found!" (err 2);
 *          wait_for_acknowledgement(1);
 *          return -1;
 *      close file;
 *      return 0;
 * 
 *  handle_connection():
 *      wait to recv data (basically infinitely);
 *      when received:
 *          send_acknowledgement();
 *      if packet received is not SEQ or is not SEQ 1:
 *          send_error_packet();
 *          return;
 *      else:
 *          send_file();
 *      return;
 * 
 *  main():
 *      open socket;
 *      handle_connection();
 */

typedef struct connection {
    struct sockaddr_storage *remote_addr;
    socklen_t addr_len;
    int socket_desc;
} connection;

int send_data(connection *connect, packet *packet, int line) {
    int return_value = (int)sendto(connect->socket_desc, packet, get_packet_size(packet), 0, (struct sockaddr *)connect->remote_addr, connect->addr_len);
    if (return_value == -1) print_error(strerror(errno), line);
    else                    print_packet(packet, 1, IS_SERVER);
    return return_value;
}

int recv_data(connection *connect, packet *packet) {
    return (int)recvfrom(connect->socket_desc, packet, sizeof(struct packet), 0, (struct sockaddr *)connect->remote_addr, &connect->addr_len);
}

int send_acknowledgement(connection *connect, packet *ack_packet, u_int ack_num) {
    // for acknowledgement:       2 is ACK packet
    set_packet_header(ack_packet, 2, 0, ack_num, 100, sizeof(packet_header));
    return send_data(connect, ack_packet, __LINE__);
}

int wait_for_acknowledgement(connection *connect, packet *send_packet, packet *recv_packet, int i) {
    int return_value;
    u_int seq_num, ack_num;
    seq_num = send_packet->header.seq_num;

    if (i < MAX_RETRIES) {  // if tried less than 8 times, wait to receive data

        return_value = recv_data(connect, recv_packet);
        if (return_value == -1) {   // if havent received data

            return_value = send_data(connect, send_packet, __LINE__); // resend data
            if (return_value == -1) return return_value;

            i = wait_for_acknowledgement(connect, send_packet, recv_packet, i+1);   // wait yet again
            if (i == -1) return i;

        } else {    // else have received data

            print_packet(recv_packet, 0, IS_SERVER);
            ack_num = recv_packet->header.seq_num;
            if (!is_packet_acknowledgement(recv_packet) || ack_num != seq_num) {   // if not the correct response

                return_value = send_data(connect, send_packet, __LINE__); // resend data
                if (return_value == -1) return return_value;

                i = wait_for_acknowledgement(connect, send_packet, recv_packet, i+1);   // wait yet again
                if (i == -1) return i;
            }
        }
        // is correct data, return
        return return_value;

    } else {    // more than maximum number of tries, return error
        print_error("Connection Closed.", __LINE__);
        return -1;
    }
}

int send_finale_packet(connection *connect, packet *send_packet, packet *recv_packet, u_int seq_num) {
    int return_value;
    // for finale packet:          3 is FIN packet
    set_packet_header(send_packet, 3, 0, seq_num, 100, sizeof(packet_header));
    return_value = send_data(connect, send_packet, __LINE__);
    if (return_value == -1) return return_value;
    return_value = wait_for_acknowledgement(connect, send_packet, recv_packet, 1);
    return return_value;
}

// any result from this is to quit, so always return -1
int send_error_packet(connection *connect, packet *send_packet, packet *recv_packet, u_int error_num) {
    // for error packet:           0 is ERR packet
    set_packet_header(send_packet, 0, error_num, 0, 100, sizeof(packet_header));
    send_data(connect, send_packet, __LINE__);
    wait_for_acknowledgement(connect, send_packet, recv_packet, 1);
    return -1;
}

int send_file(connection *connect, packet *send_packet, packet *recv_packet) {
    int return_value;
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

                return_value = send_data(connect, send_packet, __LINE__);
                if (return_value == -1) return return_value;

                return_value = wait_for_acknowledgement(connect, send_packet, recv_packet, 1);
                if (return_value == -1) return return_value;

                buffNum = 0;
                memset(send_packet->buff, 0, MAX_BUFFER_SIZE);
            }
        }
        fclose(file);
        
        seq_num++;
        // for sequence packet:        1 is SEQ packet
        set_packet_header(send_packet, 1, 0, seq_num, 100, strlen((char *)send_packet->buff));

        return_value = send_data(connect, send_packet, __LINE__);
        if (return_value == -1) return return_value;

        return_value = wait_for_acknowledgement(connect, send_packet, recv_packet, 1);
        if (return_value == -1) return return_value;

        memset(send_packet->buff, 0, MAX_BUFFER_SIZE);
        return_value = send_finale_packet(connect, send_packet, recv_packet, seq_num);
        if (return_value == -1) return return_value;

    } else {
        //file doesn't exist
        print_error(strerror(errno), __LINE__);                  // 2 is File Not Found
        return send_error_packet(connect, send_packet, recv_packet, 2);
    }

    return 0;
}

int handle_connection(int socket_desc, time_t *start) {
    int return_value;
    u_int seq_num;

    struct sockaddr_storage remote_addr;
    socklen_t addr_len = sizeof(remote_addr);

    connection connect;

    packet send_packet = init_packet();
    packet recv_packet = init_packet();

    // set connection data
    connect.remote_addr = &remote_addr;
    connect.addr_len = addr_len;
    connect.socket_desc = socket_desc;
    
    while (1) {
        return_value = recv_data(&connect, &recv_packet);
        if (return_value == -1) {
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
    return_value = send_acknowledgement(&connect, &send_packet, seq_num);
    if (return_value == -1) return return_value;

    if (!is_packet_sequence(&recv_packet) || seq_num != 1) {
        return send_error_packet(&connect, &send_packet, &recv_packet, 1);
    } else {                                                        // 1 is Bad Request
        return_value = send_file(&connect, &send_packet, &recv_packet);
        return return_value;
    }
}

int main(int argc, char *argv[]) {
    int return_value = 0;
    char * MY_PORT;

    int socket_desc;
    struct addrinfo hints, *servInfo, *p;

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
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    return_value = getaddrinfo(NULL, MY_PORT, &hints, &servInfo);
    if (return_value != 0) {
        print_error(strerror(errno), __LINE__);
        return return_value;
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
        return_value = bind(socket_desc, p->ai_addr, p->ai_addrlen);
        if (return_value == -1) {
            close(socket_desc);
            print_error(strerror(errno), __LINE__);
            continue;
        }
        break;
    }

    return_value = setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));
    if (return_value == -1 && p == NULL) {
        print_error(strerror(errno), __LINE__);
        return return_value;
    }

    freeaddrinfo(servInfo);

    return_value = handle_connection(socket_desc, &start);
    end = time(NULL);
    printf("\nTime elapsed: %ld\n", end-start);
    close(socket_desc);

    return return_value;
}

