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
#include "bencode.h"
#include "client_bencoding.h"
#include "client.h"

/* references this library: https://github.com/willemt/heapless-bencode */

void bencode_metafile(char *file_buf, int char_index, MetaFile *metafile_message) {

    //initialize bencoding
    bencode_t ben, ben2, ben3, ben4, ben5, ben6;
    const char *ren, *ren2, *ren3, *ren4, *ren5;
    int len, len2, len3, len4, len5, len6;
    long int val, val2;
    long int total_val2=0;
    const char *st;
    int first_file_name = 0;

    bencode_init(&ben, file_buf, char_index);

    while (bencode_dict_has_next(&ben)) {
        bencode_dict_get_next(&ben, &ben2, &ren, &len);
        if (strncmp(ren, "announce", len) == 0) {
            bencode_string_value(&ben2, &ren2, &len2);
            metafile_message->announce = malloc(len2+1);
            strncpy(metafile_message->announce, ren2, len2);
            metafile_message->announce[len2] = '\0';
            metafile_message->announce_length = len2;
        } else if (strncmp(ren, "info", len) == 0) {
            bencode_dict_get_start_and_len(&ben2, &st,&len6);
            value_of_info_key_length = len6;
            value_of_info_key = malloc(len6);
            memcpy(value_of_info_key, st, len6);
            while (bencode_dict_has_next(&ben2)) {
                bencode_dict_get_next(&ben2, &ben3, &ren2, &len2);
                // in the case that there are multiple files
                if (strncmp(ren2, "files", len2) == 0) {
                    while (bencode_list_has_next(&ben3)) {
                        bencode_list_get_next(&ben3, &ben4);
                        while (bencode_dict_has_next(&ben4)) {
                            bencode_dict_get_next(&ben4, &ben5, &ren4, &len4);
                            if (strncmp(ren4, "length", len4) == 0) {
                                bencode_int_value(&ben5, &val2);
                                total_val2 += val2;
                                metafile_message->total_length = total_val2;
                            } else if (strncmp(ren4, "path", len4) == 0 && first_file_name == 0) {
                                bencode_list_get_next(&ben5, &ben6);
                                bencode_string_value(&ben6, &ren5, &len5);
                                final_filename = malloc(len5+1);
                                strncpy(final_filename, ren5, len5);
                                final_filename[len5] = '\0';
                                first_file_name = 1;
                            }
                        }
                    }
                } else if (strncmp(ren2, "piece length", len2) == 0) {
                    bencode_int_value(&ben3, &val);
                    metafile_message->piece_length = val;
                } else if (strncmp(ren2, "pieces", len2) == 0) {
                    bencode_string_value(&ben3, &ren3, &len3);
                    metafile_message->hash_list = malloc(len3+1);
                    memcpy(metafile_message->hash_list, ren3, len3);
                    metafile_message->hash_list_length = len3;

                // in the case that it is only a single file
                } else if (strncmp(ren2, "length", len2) == 0) {
                    bencode_int_value(&ben3, &val2);
                    metafile_message->total_length = val2;
                } else if (strncmp(ren2, "path", len2) == 0) {
                    bencode_string_value(&ben3, &ren3, &len3);
                    final_filename = malloc(len3+1);
                    strncpy(final_filename, ren3, len3);
                    final_filename[len3] = '\0';
                }
            }
        }
    }

    //update metafile_message->announce address (getting rid of http:// and gets rid of :6969)
    char *temp = metafile_message->announce;
    tracker_ip = malloc(100);
    int start = 0;
    int index = 0;
    for (int i = 1; i<metafile_message->announce_length; i++) {
        if (temp[i-1] == ':' && start == 1) {
            break;
        }
        if (temp[i-1] == '/' && temp[i] == '/') {
            start = 1;
            continue;
        } 
        if (start == 1 && temp[i] != '/' && temp[i] != ':') {
            tracker_ip[index++] = temp[i];
        }
    }
    tracker_ip[index] = '\0';

    //Print metafile values
    //printf("string_announce: %s with length: %d\n", metafile_message->announce, metafile_message->announce_length);
    //printf("total length: %ld\n", metafile_message->total_length);
    //printf("piece length: %ld\n", metafile_message->piece_length);
    //printf("hash list: ");
    //fwrite(metafile_message->hash_list, 1, metafile_message->hash_list_length, stdout);
    //printf("with length: %d\n", metafile_message->hash_list_length);
}

void bencode_tracker_response(char *file_buf, int size, TrackerResponse *tracker_response) {
    // untested code
    int final_index = 0;
    for (int i = 3; i<size; i++) {
        if (file_buf[i-3] == '\r' && file_buf[i-2] == '\n' && file_buf[i-1] == '\r' && file_buf[i] == '\n') {
            final_index = i+1;
            break;
        }
    }

    //initialize bencoding
    bencode_t ben, ben2, ben3;
    const char *ren, *ren2, *ren3;
    int len, len2, len3;
    long int val, val2, val3, val4, val5;

    bencode_init(&ben, &file_buf[final_index], size-final_index);

    while (bencode_dict_has_next(&ben)) {
        bencode_dict_get_next(&ben, &ben2, &ren, &len);
        if (strncmp(ren, "complete", len) == 0) {
            bencode_int_value(&ben2, &val);
            tracker_response->complete = val;
        } else if (strncmp(ren, "downloaded", len) == 0) {
            bencode_int_value(&ben2, &val2);
            tracker_response->downloaded = val2;
        } else if (strncmp(ren, "incomplete", len) == 0) {
            bencode_int_value(&ben2, &val3);
            tracker_response->incomplete = val3;
        } else if (strncmp(ren, "interval", len) == 0) {
            bencode_int_value(&ben2, &val4);
            tracker_response->interval = val4;
        } else if (strncmp(ren, "peers", len) == 0) {
            tracker_response->peers = malloc(50 * 8);
            for (int i = 0; i < 50; i++) {
                tracker_response->peers[i] = NULL;
            }
            tracker_response->peers_length = 0;
            //peers is a dictionary
            if (bencode_is_dict(&ben2)) {
                compact = 0;
                while(bencode_dict_has_next(&ben2)) {
                    bencode_dict_get_next(&ben2, &ben3, &ren2, &len2);
                    Peer *curr_peer = malloc(sizeof(Peer));
                    if (strncmp(ren2, "peer id", len2) == 0) {
                        bencode_string_value(&ben3, &ren2, &len2);
                        curr_peer->peer_id = malloc(len2+1);
                        strncpy(curr_peer->peer_id, ren2, len2);
                        curr_peer->peer_id[len2] = '\0';
                    } else if (strncmp(ren2, "ip", len2) == 0) {
                        bencode_string_value(&ben3, &ren2, &len2);
                        memcpy(&curr_peer->ip, ren2, 4);
                    } else if (strncmp(ren2, "port", len2) == 0) {
                        bencode_int_value(&ben3, &val5);
                        curr_peer->port = val5;
                    }
                    tracker_response->peers[tracker_response->peers_length] = curr_peer;
                    tracker_response->peers_length = tracker_response->peers_length +1;
                }
            //compact format of peers
            } else if (bencode_is_string(&ben2)){
                compact = 1;
                bencode_string_value(&ben2, &ren3, &len3);
                char *compact_peers = malloc(len3+1);
                strncpy(compact_peers, ren3, len3);
                compact_peers[len3] = '\0';
                assign_compact_peers(compact_peers, len3, tracker_response);
            }
        } 
    }

    //Print metafile values
    printf("interval: %d\n", tracker_response->interval);
    printf("complete: %d\n", tracker_response->complete);
    printf("incomplete: %d\n", tracker_response->incomplete);
    printf("downloaded: %d\n", tracker_response->downloaded);
    printf("peers_length: %d\n", tracker_response->peers_length);
    printf("PEER LIST: \n");
    for (int i = 0; i < tracker_response->peers_length; i++) {
        if (tracker_response->peers[i] != NULL) {
            printf("peer id: %s, peer ip in network byte: %d, peer port: %d\n", tracker_response->peers[i]->peer_id, tracker_response->peers[i]->ip, be16toh(tracker_response->peers[i]->port));
        }
    }

}

// used in the case we receive the compact format of peers from tracker bencoded response
void assign_compact_peers(char *compact, int size, TrackerResponse *tracker_response) {
    tracker_response->peers = malloc(50 * 8);
    for (int i = 0; i < 50; i++) {
        tracker_response->peers[i] = NULL;
    }
    tracker_response->peers_length = 0;
    for (int i = 0; i<size; i+=6) {
        Peer *curr_peer = malloc(sizeof(Peer));
        memcpy(&curr_peer->ip, &compact[i], 4);
        memcpy(&curr_peer->port, &compact[i+4], 2);
        //no id
        curr_peer->peer_id = NULL;
        tracker_response->peers[tracker_response->peers_length] = curr_peer;
        tracker_response->peers_length = tracker_response->peers_length +1;
    }
}

int get_tracker_port (char *announce, int announce_length) {
    char *port_char = malloc(10);
    int start = 0;
    int index = 0;
    int count = 0;
    for (int i = 0; i<announce_length; i++) {
        if (announce[i] == ':') {
            start = 1;
            count++;
        } else if (start == 1 && count ==2 && announce[i] != '/') {
            port_char[index++] = announce[i];
        }
    }
    port_char[index] = '\0';
    return atoi(port_char);
}