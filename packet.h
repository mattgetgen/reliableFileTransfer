/**
 * @file packet.h
 * @author Matthew Getgen (matt_getgen@taylor.edu)
 * @brief relaible file transfer packet and related functions
 * @version 0.1
 * @date 2022-03-30
 */

#ifndef PACKET_H
#define PACKET_H

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 1408
#define MAX_RETRIES 8

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;

/*
 * packet_header Design:
 * 
 * u_char info:
 *   0  1  2  3  4  5  6  7
 *  |  A  |  B  |     C     |
 *  |2Bits|2Bits|  4 Bits   |
 * 
 *  A: Packet Type
 *   - 00: ERR packet (error)
 *   - 01: SEQ packet (sequence)
 *   - 10: ACK packet (acknowledgement)
 *   - 11: FIN packet (finale)
 * 
 *  B: Error Message Type
 *   - 00: No Error
 *   - 01: Bad Request
 *   - 10: File Not Found
 *   - 11: Unknown/Unhandled Error
 * 
 *  C: Header Size (in bytes) (int this case, it's always 8, but if its ever over 15 this should be removed)
 * 
 * u_char percent
 * u_short data_size
 * u_int seq_num
 * 
 * Total Size: 8 Bytes
 */

typedef struct packet_header {
    u_char info;
    u_char percent;
    u_short data_size;
    u_int seq_num;
} packet_header;

typedef struct Packet {
    packet_header header;
    u_char buff[MAX_BUFFER_SIZE];
} Packet;

/**
 * Initialize the values stored in a packet
 */
Packet init_packet(void);

/**
 * Set the values stored in a packet header.
 */
void set_packet_header(Packet *packet, u_int type, u_int error, u_int seq_num, u_int percent, u_short data_size);

/**
 * Returns the type number stored in the packet.
 */
u_int get_packet_type(Packet *packet);

/**
 * Returns the error number stored in the packet.
 */
u_int get_packet_error(Packet *packet);

/**
 * Returns the full packet size of a packet.
 */
u_short get_packet_size(Packet *packet);

/**
 * Returns true if the packet is an error packet.
 */
int is_packet_error(Packet *packet);

/**
 * Returns true if the packet is a sequence packet.
 */
int is_packet_sequence(Packet *packet);

/**
 * Returns true if the packet is an acknowledgement packet.
 */
int is_packet_acknowledgement(Packet *packet);

/**
 * Return true if the packet is a finale packet.
 */
int is_packet_finale(Packet *packet);

/**
 * Prints packet information to the console.
 */
void print_packet(Packet *packet, int isSend, int isServer);

/**
 * Prints errors to the console.
 */
void print_error(char *err, int line);

/**
 * Prints specific error messages to the console.
 */
void print_error_msg(Packet *packet, int line);

#endif