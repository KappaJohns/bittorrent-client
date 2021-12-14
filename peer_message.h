#ifndef PEER_MESSAGE_H
#define PEER_MESSAGE_H

void send_interested(int sock);
void send_notinterested(int sock);
void send_choked(int sock);
void send_notchoked(int sock);
void send_have(int sock, uint32_t piece_index);
void send_bitfield(int sock);
void send_request(int sock, uint32_t index, uint32_t begin, uint32_t length);
void send_piece(int sock, uint32_t index, uint32_t begin, uint8_t *block, int block_length);
void cancel(int sock, uint32_t index, uint32_t begin, uint32_t length);
void keep_alive();
void send_port(int sock, uint16_t port);
#endif