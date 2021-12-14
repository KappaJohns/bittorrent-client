#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

//metafile structure
typedef struct metafile {
    char *announce;
    int announce_length;
    long int total_length;
    long int piece_length;
    uint8_t *hash_list;
    int hash_list_length;
} MetaFile;
MetaFile metafile_message;

//tracker message
typedef struct trackermessage {
    char *info_hash;
    char *peer_id;
    int port;
    long int left;
    long int uploaded;
    long int downloaded;
    char *event;
} TrackerMessage;
TrackerMessage tracker_message;

//peer struct
typedef struct peer {
    char *peer_id;
    uint32_t ip;
    uint16_t port;
    int sock;
    int finished_handshake;
    int am_interested;
    int am_choking;
    int peer_choking;
    int peer_interested;
    time_t start;
    time_t end;
    int bandwidth;
    uint8_t *bitfield;
    double total_time;
    long int total_bytes;
    time_t last_keep_alive_time;
} Peer;

//tracker response
typedef struct trackerresponse {
    int interval;
    int complete;
    int incomplete;
    int downloaded;
    Peer **peers;
    int peers_length;  
} TrackerResponse;

//handshake message
typedef struct handshakemessage {
    uint8_t pstrlen;
    char *pstr;
    uint64_t reserved;
    uint8_t *info_hash;
    char *peer_id;
} HandshakeMessage;

typedef struct piece {
    FILE *fp;
    int begin;
    uint8_t isComplete;
} Piece;

//fields of this client
int compact;
char *tracker_ip;
uint8_t *peer_id;
uint8_t *info_hash;
int tracker_port;
Peer **peers_list;
uint8_t *value_of_info_key;
int value_of_info_key_length;
int *socket_list;
int totalSockets;

// indicates to us if we are a seeder (1) or not a seeder (0)
int downloaded_all;

int uploaded;
int downloaded;
int left;

uint8_t *bitfield;
int num_pieces;
int bitfield_length;
time_t last_piece_time;

Piece **piece_list;
TrackerResponse curr_tracker_response;

char *host;
char *slash_URL;
char *this_host_ip;
char *ip_arg;
int have_ip_arg;
int port_arg;

char *final_filename;

//helper functions
Peer *find_peer_peerlist(int sock);
void remove_sock_peerlist(int sock);
void add_peer_peerlist_finished_handshake(int sock, uint16_t ip, uint32_t port, char *peer_id);
void add_peer_peerlist(int sock, uint16_t ip, uint32_t port, char *peer_id);
void update_socket_list();
void accept_incoming();
void peer_message(int i);
void tracker_req_interval();

int compare_bandwidth(Peer *p1, Peer *p2);
void keep_alive_check();
void unchoke_peers_list(Peer ** list, int *num_peers);
void unchoke_choke_peers(Peer **unchoke_list, int num_peers);
void reconnect_peers();

#endif