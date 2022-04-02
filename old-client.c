#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <netdb.h>

#define MAX_BUFFER_SIZE 1458
#define MAX_RETRIES 8

/*
 * SEQ packet header:
 * 0  1  2  3  4  5  6  7     ...
 * | SEQ  NUM | DATA SIZE |   DATA   |
 * | 4  Bytes | 4  Bytes  | ?  Bytes |
 *
 * ACK packet header:
 * 0  1  2  3
 * | ACK  NUM |
 * | 4  Bytes |
 *
 * FIN packet header:
 * 0  1  2  3  4  5  6  7     ...
 * | NULL NUM | DATA SIZE | FIN  MSG |
 * | 4  Bytes | 4  Bytes  | ?  Bytes |
 *
 * ERR packet header:
 * 0  1  2  3  4  5  6  7     ...
 * | NULL NUM | DATA SIZE | ERR  MSG |
 * | 4  Bytes | 4  Bytes  | ?  Bytes |
 *
 * send_acknowledgement():
 *      make ACK header with ACK NUM from SEQ num;
 *      send header to server;
 *
 * wait_for_acknowledgement():
 *      seq_num = char_to_int(data);
 *      while i < MAX_RETRIES:
 *          wait for 2 seconds;
 *              recv ack;
 *          if no recv in 2 seconds:
 *              resend data;
 *          else:
 *              ack_num = char_to_data(recv);
 *              if seq_num == ack_num:
 *                  break;
 *              else:
 *                  resend data;
 *          i++;
 *      clear(recv);
 *
 * handle_connection():
 *      make SEQ header with SEQ NUM (1), and data size. add server file to request;
 *      send file request;
 *      wait_for_acknowledgement();
 *
 *      while i < MAX_RETRIES:
 *          recv data;
 *          store seq_num;
 *          if seq_num == next:
 *              send_acknowledgement();
 *              read data;
 *              write data into local file;
 *          else if next = seq_num*2:
 *              close file;
 *              return 0;
 *          else if seq_num == 0:
 *              print error message;
 *      close file;
 *
 * main():
 *      open socket;
 *      handle_connection();
 */

// convert SEQ and ACK number from int to char[]
void int_to_char(unsigned char * data, unsigned int a) {
    data[0] = (a >> 24) & 0xFF;
    data[1] = (a >> 16) & 0xFF;
    data[2] = (a >> 8) & 0xFF;
    data[3] = a & 0xFF;
}

// convert SEQ and ACK number from char[] to int
unsigned int char_to_int(const unsigned char * data) {
    unsigned int a = (unsigned int)(
            (unsigned char)(data[0]) << 24 |
            (unsigned char)(data[1]) << 16 |
            (unsigned char)(data[2]) << 8  |
            (unsigned char)(data[3]));
    return a;
}

/*
 * send_acknowledgement():
 *      make ACK header with ACK NUM from SEQ num;
 *      send header to server;
 */

int send_acknowledgement(int socket_desc, struct addrinfo * p, unsigned int ack_num) {
    int return_value;
    unsigned char data[4];
    memset(data, 0, sizeof(data));

    int_to_char(data, ack_num);

    return_value = (int)sendto(socket_desc, data, sizeof(data), 0, p->ai_addr, p->ai_addrlen);
    if (return_value == -1) {
        printf("Error: %s (line: %d)\n", strerror(errno), __LINE__);
        return return_value;
    }
    printf("client -> ACK %d -> server\n", ack_num);

    memset(data, 0, sizeof(data));
    return 0;
}

/*
 * wait_for_acknowledgement():
 *      seq_num = char_to_int(data);
 *      while i < MAX_RETRIES:
 *          wait for 2 seconds;
 *              recv ack;
 *          if no recv in 2 seconds:
 *              resend data;
 *          else:
 *              ack_num = char_to_data(recv);
 *              if seq_num == ack_num:
 *                  break;
 *              else:
 *                  resend data;
 *          i++;
 *      clear(recv);
 */

int wait_for_acknowledgement(int socket_desc, struct addrinfo * p, unsigned char * data) {
    int return_value, i = 0;

    unsigned char * cursor;
    unsigned int seq_num, ack_num, data_size;
    seq_num = char_to_int(data);
    cursor = &data[4];
    data_size = char_to_int(cursor)+8;      // amount to send, accommodating for 8 byte header.

    unsigned char recv[MAX_BUFFER_SIZE];
    memset(recv, 0, MAX_BUFFER_SIZE);

    while (i < MAX_RETRIES) {
        return_value = (int)recvfrom(socket_desc, recv, MAX_BUFFER_SIZE, 0, p->ai_addr, &p->ai_addrlen);
        if (return_value == -1) {
            // resend data
            return_value = (int)sendto(socket_desc, data, data_size, 0, p->ai_addr, p->ai_addrlen);
            if (return_value == -1) {
                printf("Error: %s (line: %d)\n", strerror(errno), __LINE__);
                return return_value;
            }
            printf("client -> SEQ %d -> server\n", seq_num);
        } else {
            ack_num = char_to_int(recv);
            if (ack_num == seq_num) {   // received correct ACK, stop
                printf("client <- ACK %d <- server\n", ack_num);
                break;
            } else {
                // resend data
                return_value = (int)sendto(socket_desc, data, data_size, 0, p->ai_addr, p->ai_addrlen);
                if (return_value == -1) {
                    printf("Error: %s (line: %d)\n", strerror(errno), __LINE__);
                    return return_value;
                }
                printf("client -> SEQ %d -> server\n", seq_num);
                i = 0;
                continue;
            }
        }
        i++;
    }
    if (i >= MAX_RETRIES) {
        printf("Error: Connection Closed.\n");
        return -1;
    }
    memset(recv, 0, MAX_BUFFER_SIZE);
    return 0;

}

/*
 * handle_connection():
 *      make SEQ header with SEQ NUM (1), and data size. add server file to request;
 *      send file request;
 *      wait_for_acknowledgement();
 *
 *      while i < MAX_RETRIES:
 *          recv data;
 *          store seq_num;
 *          if seq_num == next:
 *              send_acknowledgement();
 *              read data;
 *              write data into local file;
 *          else if next = seq_num*2:
 *              close file;
 *              return 0;
 *          else if seq_num == 0:
 *              print error message;
 *      close file;
 */

int handle_connection(int socket_desc, struct addrinfo * p, char * remote_file, char * local_file) {
    FILE *fp;
    int return_value, i = 0;
    unsigned int temp, seq_num = 1, recv_size;

    unsigned char * cursor;
    unsigned char data[MAX_BUFFER_SIZE];
    unsigned char recv[MAX_BUFFER_SIZE];
    memset(data, 0, MAX_BUFFER_SIZE);
    memset(recv, 0, MAX_BUFFER_SIZE);

    // make first seq header with requesting file
    int_to_char(data, seq_num);
    cursor = &data[4];
    int_to_char(cursor, strlen(remote_file));
    cursor = &data[8];
    memcpy(cursor, remote_file, strlen(remote_file));

    return_value = (int)sendto(socket_desc, data, strlen(remote_file)+8, 0, p->ai_addr, p->ai_addrlen);
    if (return_value == -1) {
        printf("Error: %s (line: %d)\n", strerror(errno), __LINE__);
        return return_value;
    }
    printf("client -> SEQ %d -> server\n", seq_num);

    return_value = wait_for_acknowledgement(socket_desc, p, data);
    if (return_value == -1) {
        return return_value;
    }

    if ((fp = fopen(local_file, "w")) == NULL) {
        printf("Error: %s (line: %d)\n", strerror(errno), __LINE__);
        return -1;
    }

    while (i < MAX_RETRIES) {
        return_value = (int)recvfrom(socket_desc, recv, MAX_BUFFER_SIZE, 0, p->ai_addr, &p->ai_addrlen);
        if (return_value == -1) {
            i++;

        } else {
            i = 0;

            temp = char_to_int(recv);
            if (temp == seq_num+1) {    // client received next seq packet
                seq_num = temp;
                printf("client <- SEQ %d <- server\n", temp);

                return_value = send_acknowledgement(socket_desc, p, seq_num);
                if (return_value == -1) {
                    return return_value;
                }
                cursor = &recv[4];
                recv_size = char_to_int(cursor);
                cursor = &recv[8];
                fwrite(cursor, recv_size, 1, fp);

            } else if (temp == seq_num*2) {
                printf("client <- FIN %d <- server\n", temp);

                return_value = send_acknowledgement(socket_desc, p, temp);
                if (return_value == -1) {
                    return return_value;
                }

                fclose(fp);
                memset(data, 0, MAX_BUFFER_SIZE);
                memset(recv, 0, MAX_BUFFER_SIZE);

                return_value = 0;
                break;

            } else if (temp == 0) {     // client received a server error.
                printf("client <- ERR %d <- server\n", temp);

                return_value = send_acknowledgement(socket_desc, p, temp);
                if (return_value == -1) {
                    return return_value;
                }
                cursor = &recv[8];
                printf("Error: %s (line: %d)\n", cursor, __LINE__);
                return_value = -1;
                break;
            }
        }
    }
    if (i >= MAX_RETRIES) {
        printf("Error: Connection Closed.\n");
    }

    return return_value;
}

int main(int argc, char *argv[]) {
    int return_value;
	char * SERVER_IP, * SERVER_PORT, * REMOTE_PATH, * LOCAL_PATH;
    char file_name[24] = "/";
    char * file_name_ptr = &file_name[1];

	int socket_desc;
	struct addrinfo hints, *servInfo, *p;

	time_t start, end;

	// command line arguments
	if (argc != 5) {
        printf("Arguments expected: <Server IP> <Server Port> <Remote Path> <Local Path>\n");
        return -2;
    } else {
        SERVER_IP = argv[1];
        SERVER_PORT = argv[2];
        REMOTE_PATH = argv[3];
        LOCAL_PATH = argv[4];
	}

	memset(&hints, 0, sizeof(hints));// set all data in struct to 0
	hints.ai_family = AF_INET;          // IPv4
	hints.ai_socktype = SOCK_DGRAM;     // UDP

	// set timer
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;


    return_value = getaddrinfo(SERVER_IP, SERVER_PORT, &hints, &servInfo);
    if (return_value != 0) {
        printf("Error: %s (line: %d)\n", strerror(errno), __LINE__);
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
            printf("Error: %s (line: %d)\n", strerror(errno), __LINE__);
            continue;
        }

        break;
    }

    return_value = setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *) &tv, sizeof(struct timeval));
    if (return_value == -1 && p == NULL) {
        printf("Error: %s (line: %d)\n", strerror(errno), __LINE__);
        return return_value;
    }

    printf("Enter a file name: ");
    scanf("%s", file_name_ptr);
    char remote_file[strlen(REMOTE_PATH)+strlen(file_name)+1];
    char local_file[strlen(LOCAL_PATH)+strlen(file_name)+1];
    memset(remote_file, 0, sizeof(remote_file));
    memset(local_file, 0, sizeof(local_file));

    strncat(remote_file, REMOTE_PATH, strlen(REMOTE_PATH));
    strncat(remote_file, file_name, strlen(file_name));
    strncat(local_file, LOCAL_PATH, strlen(LOCAL_PATH));
    strncat(local_file, file_name, strlen(file_name));

    freeaddrinfo(servInfo);

    start = time(NULL);
    return_value = handle_connection(socket_desc, p, remote_file, local_file);
    end = time(NULL);
    printf("Time elapsed: %ld\n", end-start);
    close(socket_desc);

    return return_value;
}

