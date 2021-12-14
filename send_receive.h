#ifndef SEND_RECEIVE_H
#define SEND_RECEIVE_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "client.h"

int send_get_req(char *host, char *slash_URL, char *ip, int port, TrackerMessage *message, char *response, int response_size);
char* urlencode(unsigned char* originalText);
void split_announce(char *announce, int length, char *host, char *slash_URL);
int get_my_sock(int port);
int connect_to_client(uint32_t ip, uint16_t port);
int receive_client_handshake(int sock, HandshakeMessage *handshake_message);
int send_client_handshake(int sock);
int send_bytes (int sendSock, void *sendType, size_t sendBytes);
int recv_bytes (int recvSock, void *recvType, size_t recvBytes);
#endif