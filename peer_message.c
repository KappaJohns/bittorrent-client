#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <endian.h>
#include "client.h"
#include "send_receive.h"
#include "stopwatch.h"
#include "files.h"

void send_interested(int sock) {
    // build a buffer to send
    char buffer[5];
    uint32_t length = htobe32(1);
    uint8_t id = 2;
    memcpy(buffer, &length, 4);
    memcpy(buffer+4, &id, 1);

    //send
    send_bytes(sock, buffer, 5);
}

void send_notinterested(int sock) {
    // build a buffer to send
    char buffer[5];
    uint32_t length = htobe32(1);
    uint8_t id = 3;
    memcpy(buffer, &length, 4);
    memcpy(buffer+4, &id, 1);

    //send
    send_bytes(sock, buffer, 5);
}

void send_choked(int sock) {
    // build a buffer to send
    char buffer[5];
    uint32_t length = htobe32(1);
    uint8_t id = 0;
    memcpy(buffer, &length, 4);
    memcpy(buffer+4, &id, 1);

    //send
    send_bytes(sock, buffer, 5);
}

void send_notchoked(int sock) {
    // build a buffer to send
    char buffer[5];
    uint32_t length = htobe32(1);
    uint8_t id = 1;
    memcpy(buffer, &length, 4);
    memcpy(buffer+4, &id, 1);

    //send
    send_bytes(sock, buffer, 5);
}

void send_have(int sock, uint32_t piece_index) {
    // build a buffer to send
    char buffer[9];
    uint32_t length = htobe32(5);
    uint8_t id = 4;
    uint32_t piece = htobe32(piece_index);
    memcpy(buffer, &length, 4);
    memcpy(buffer+4, &id, 1);
    memcpy(buffer+4+1, &piece, 4);
    
    //send
    send_bytes(sock, buffer, 9);
    // puts("Sending have");
}

void send_bitfield(int sock) {
    // build a buffer to send
    
    char buffer[4+1+bitfield_length];
    uint32_t length = htobe32(1+bitfield_length);
    uint8_t id = 5;
    memcpy(buffer, &length, 4);
    memcpy(buffer+4, &id, 1);
    memcpy(buffer+4+1, bitfield, bitfield_length);

    //send
    send_bytes(sock, buffer, 5+bitfield_length);
}

void send_request(int sock, uint32_t index, uint32_t begin, uint32_t length) {
    // build a buffer to send
    char buffer[17];
    int total_length = htobe32(13);
    uint8_t id = 6;
    int send_index = htobe32(index);
    int send_begin = htobe32(begin);
    int send_length = htobe32(length);
    memcpy(buffer, &total_length, 4);
    memcpy(buffer+4, &id, 1);
    memcpy(buffer+4+1, &send_index, 4);
    memcpy(buffer+4+4+1, &send_begin, 4);
    memcpy(buffer+4+4+4+1, &send_length, 4);

    //send
    send_bytes(sock, buffer, 17); 
}

void send_piece(int sock, uint32_t index, uint32_t begin, uint8_t *block, int block_length) {
    // build a buffer to send
    char buffer[13+block_length];
    uint32_t length = htobe32(9+block_length);
    uint8_t id = 7;
    uint32_t send_index = htobe32(index);
    uint32_t send_begin = htobe32(begin);
    memcpy(buffer, &length, 4);
    memcpy(buffer+4, &id, 1);
    memcpy(buffer+4+1, &send_index, 4);
    memcpy(buffer+4+4+1, &send_begin, 4);
    memcpy(buffer+4+4+4+1, block, block_length);

    //send
    send_bytes(sock, buffer, 13+block_length);
}

void cancel(int sock, uint32_t index, uint32_t begin, uint32_t length) {
    // build a buffer to send
    char buffer[17];
    uint32_t total_length = htobe32(13);
    uint8_t id = 8;
    uint32_t send_index = htobe32(index);
    uint32_t send_begin = htobe32(begin);
    uint32_t send_length = htobe32(length);
    memcpy(buffer, &total_length, 4);
    memcpy(buffer+4, &id, 1);
    memcpy(buffer+4+1, &send_index, 4);
    memcpy(buffer+4+4+1, &send_begin, 4);
    memcpy(buffer+4+4+4+1, &send_length, 4);

    //send
    send_bytes(sock, buffer, 17); 
}

void send_port(int sock, uint16_t port) {
    char buffer[7];
    int length = htobe32(3);
    uint8_t id = 9;
    int send_port = htobe16(port);

    memcpy(buffer, &length, 4);
    memcpy(buffer+4, &id, 1);
    memcpy(buffer+4+1, &send_port, 2);

    send_bytes(sock, buffer, 7);


}

void keep_alive() {
    // build the keep_alive packet
    char request[4];
    int len = htobe32(0);
    memcpy(request, &len, 4);

    for (int i=0;i<50;i++) {
        if (peers_list[i] != NULL) {
            // peer must have to perform handshake first before any messages
            if (peers_list[i]->sock >= 0 && peers_list[i]->finished_handshake == 1) {
                if (send_bytes(peers_list[i]->sock, request, 4) <0 ) {
                    printf("Error sending keep alive to socket %d\n", peers_list[i]->sock);
                }
            }
        }
    }
}