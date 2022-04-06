/**
 * @file packet.c
 * @author Matthew Getgen (matt_getgen@taylor.edu)
 * @brief relaible file transfer packet and related functions
 * @version 0.1
 * @date 2022-03-30
 */
#include "packet.h"

Packet init_packet(void) {
    Packet new_packet;
    new_packet.header.info = 0x00;
    new_packet.header.percent = 0x00;
    new_packet.header.data_size = 0;
    new_packet.header.seq_num = 0;
    memset(new_packet.buff, 0, MAX_BUFFER_SIZE);
    return new_packet;
}

void set_packet_header(Packet *packet, u_int type, u_int error, u_int seq_num, u_int percent, u_short data_size) {
    u_char temp;
    temp =        (u_char) ( (type << 6)           & 0xC0 );    // 0xC0  is  1100 0000
    temp = temp | (u_char) ( (error << 4)          & 0x30 );    // 0x30  is  0011 0000
    temp = temp | (u_char) ( sizeof(packet_header) & 0x0F );    // 0x0F  is  0000 1111
    packet->header.info = temp;
    packet->header.percent = (u_char)percent;
    packet->header.seq_num = seq_num;
    packet->header.data_size = data_size;
    return;
}

u_int get_packet_type(Packet *packet) {
    return (u_int) ( (packet->header.info & 0xC0) >> 6 );
}                                        // 0xC0  is  1100 0000

u_int get_packet_error(Packet *packet) {
    return (u_int) ( (packet->header.info & 0x30) >> 4 );
}                                        // 0x30  is  0011 0000

u_short get_packet_size(Packet *packet) {
    return (u_short) ( sizeof(packet_header) + packet->header.data_size );
}

int is_packet_error(Packet *packet) {
    return ( get_packet_type(packet) == 0 );
}

int is_packet_sequence(Packet *packet) {
    return ( get_packet_type(packet) == 1 );
}

int is_packet_acknowledgement(Packet *packet) {
    return ( get_packet_type(packet) == 2 );
}

int is_packet_finale(Packet *packet) {
    return ( get_packet_type(packet) == 3 );
}

void print_packet(Packet *packet, int isSend, int isServer) {
    char *packet_type;
    char *arrow_dir;
    u_int type = get_packet_type(packet);
    if      (type == 0) packet_type = "ERR";
    else if (type == 1) packet_type = "SEQ";
    else if (type == 2) packet_type = "ACK";
    else if (type == 3) packet_type = "FIN";
    
    if (isSend) arrow_dir = "->";
    else        arrow_dir = "<-";

    if (isServer) printf("\nserver %s %s %d %s client", arrow_dir, packet_type, packet->header.seq_num, arrow_dir);
    else printf("\nclient %s %s %d %s server", arrow_dir, packet_type, packet->header.seq_num, arrow_dir);
    return;
}

void print_error(char *err, int line) {
    printf("\nError: %s (line: %d)", err, line);
    return;
}

void print_error_msg(Packet *packet, int line) {
    int error = get_packet_error(packet);
    if      (error == 1) print_error("Bad Request!", line);
    else if (error == 2) print_error("file Not Found!", line);
    else if (error == 3) print_error("Unknown/Unhandled Error.", line);
    else                 printf("\nNo Error Present.");
    return;
}

