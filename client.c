#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <endian.h>
#include <math.h>
#include "client_parser.h"
#include "bencode.h"
#include "client.h"
#include "client_bencoding.h"
#include "bigint.h"
#include "hash.h"
#include "send_receive.h"
#include "bitfield.h"
#include "peer_message.h"
#include "stopwatch.h"
#include "piece.h"
#include "files.h"

#define REQUEST_BYTE_CAP 10000

void handleSignals(int sig) {
    printf("Caught signal %d\n", sig);
    fflush(stdout);
    exit(sig);
}

int main(int argc, char *argv[]) {
    signal(SIGHUP, handleSignals);
    // signal(SIGINT, handleSignals);
    signal(SIGQUIT, handleSignals);
    signal(SIGILL, handleSignals);
    signal(SIGTRAP, handleSignals);
    signal(SIGABRT, handleSignals);
    // we do not have all the pieces
    downloaded_all = 0;

    // parse the options for the torrentfile
    struct client_arguments parsed_arguments = client_parseopt(argc, argv);
    char *filename = parsed_arguments.filename;
    ip_arg = parsed_arguments.ip_address;
    port_arg = parsed_arguments.port;

    if (ip_arg == NULL) {
        have_ip_arg = 0;
    } else {
        have_ip_arg = 1;
    }
 
    // Retreiving host name and host ip to send to tracker
    // NOTE: THIS IS NOT THE EXTERNAL IP
    char host_buffer[256]; 
    gethostname(host_buffer, sizeof(host_buffer));
    struct hostent *host_info = gethostbyname(host_buffer);
    this_host_ip = malloc(20);
    memcpy(this_host_ip, inet_ntoa(*((struct in_addr*)host_info->h_addr_list[0])), 20);
    printf("My Host IP: %s\n", this_host_ip);
    if (have_ip_arg == 1) {
         printf("My External IP: %s\n", ip_arg);
    }

    // ---------------------READ TORRENT FILE------------------
    FILE *file = fopen (filename, "r");
    if (file == NULL) {
        puts ("failed to open file");
        exit(1);
    }
    free(filename);

    int c;
    int char_index = 0;
    int size = 10000;
    char *file_buf = malloc(size);
    if(file_buf == NULL) {
        puts ("failed file_Buf malloc");
        exit(1);
    }
    while ((c = fgetc(file)) != EOF) {
        if(char_index >= size-1) {
            // increase the size of file_buf
            size += 1000;
            file_buf = realloc(file_buf, size);
            if(file_buf == NULL) {
                puts ("failed file_buf realloc");
                exit(1);
            }
        }
        file_buf[char_index++] = c;
    }
    file_buf[char_index++] = '\0';

    //Prints the file we just read in
    //fwrite(file_buf, 1, char_index, stdout);
    //printf("\n");

    //bencode the metafile and store it in metafile_message
    bencode_metafile(file_buf, char_index, &metafile_message);

    //free memory for the file
    free(file_buf);
    free(file);
    //create file storage
    if(createdir() !=1 ){
        puts("failed to create directory");
    }
    // uint8_t filetest[12] = { 0x00,0x01,0x00,0x00,0x00,0x06,0xFE,0x03,0x01,0xC1,0x00,0x01 };
    // createfile(filetest,12,"0");
    // uint8_t filetest2[12] = { 0x01,0x01,0x00,0x00,0x00,0x06,0xFE,0x03,0x01,0xC1,0x00,0x01 };
    // createfile(filetest2,12,"1");
    // uint8_t filetest3[12];
    // readFile(filetest3,0,12,1);
    // printf("filetest3\n");
    // for(int i = 0; i<12; i++){
    //     printf("%d\n",filetest3[i]);
    // }
    // combineallfiles(1,12);
    // ---------------------INITIALIZE BITFIELD------------------
    
    num_pieces = metafile_message.hash_list_length/20;
    bitfield_length = ceil((double)num_pieces/(double)8);
    bitfield = malloc(bitfield_length);
    piece_list = malloc(bitfield_length*8);
    for (int i =0; i<bitfield_length;i++) {
        piece_list[i] = NULL;
    }
    initialize_bitfield(bitfield, bitfield_length);
    getbitfieldfromfiles(bitfield, num_pieces, metafile_message.piece_length);
    // for (int i = 0; i < num_pieces; i++){
    //     printf("%02X ", bitfield[i]);
    // }
    // printf("\n");
    
    // ---------------------- BEGIN CONTACTING TRACKER------------------
    
    //initializes variables for the first tracker message to be sent
    info_hash = malloc(20);
    unsigned char *temp_hashed_info_hash = malloc(21);
    tracker_message.info_hash = malloc(21);
    sha1_hash(info_hash, value_of_info_key, value_of_info_key_length);
    memcpy(temp_hashed_info_hash, info_hash, 20);
    temp_hashed_info_hash[20] = '\0';
    tracker_message.info_hash = urlencode(temp_hashed_info_hash);
    free(temp_hashed_info_hash);
    srand(time(NULL));
    peer_id = malloc(20);
    tracker_message.peer_id = malloc(21);
    for (int i = 0; i< 20; i++) {
        tracker_message.peer_id[i] = 'A' + (rand() % 26);
    }
    memcpy(peer_id, tracker_message.peer_id, 20);
    tracker_message.peer_id[20] = '\0';

    // setting the global variables as well
    left = abs(metafile_message.total_length);
    uploaded = 0;
    downloaded = 0;

    tracker_message.port = parsed_arguments.port;
    tracker_message.left = left;
    tracker_message.uploaded = uploaded;
    tracker_message.downloaded = downloaded;
    tracker_message.event = malloc(10);
    strncpy(tracker_message.event, "started", 8);
    tracker_message.event[7] = '\0';


    // get the remaining /URL and then the host address 
    host = malloc(metafile_message.announce_length);
    slash_URL = malloc(metafile_message.announce_length);
    split_announce(metafile_message.announce, metafile_message.announce_length, host, slash_URL);

    //send get request to tracker with tracker_ip and tracker_port
    // stores the response as an arbitrary size
    int response_size = 4096;
    char response[response_size];
    tracker_port = get_tracker_port(metafile_message.announce, metafile_message.announce_length);
    while (send_get_req(host, slash_URL, tracker_ip, tracker_port, &tracker_message, response, response_size) == 0) {
        puts("failed to send intiial request to tracker");
        // exit(1);
    }

    // ----------------------- RECEIVED TRACKER RESPONSE-------------------

    //initializes the tracker_response, bencodes and sets the appropriate fields of the tracker_response
    TrackerResponse tracker_response;
    bencode_tracker_response(response, response_size, &tracker_response);

    // gets the client socket
    int client_sock = get_my_sock(parsed_arguments.port);
    if (client_sock == -1) {
        puts("failed to create client socket");
        exit(0);
    }

    // makes a connection to all the peers
    peers_list = malloc(50 * 8);
    for (int i = 0; i<50; i++) {
        peers_list[i] = NULL;
    }
    socket_list = malloc(51 * sizeof(int));
    totalSockets = 0;

    socket_list[totalSockets++] = client_sock;

    for (int i = 0; i< tracker_response.peers_length; i++) {
        if (tracker_response.peers[i] != NULL && tracker_response.peers[i]->port != 0) {
            int peer_sock = connect_to_client(tracker_response.peers[i]->ip, tracker_response.peers[i]->port);
            if (peer_sock != -1) {
                /*add_peer_peerlist(peer_sock, tracker_response.peers[i]->ip, tracker_response.peers[i]->port, tracker_response.peers[i]->peer_id);
                socket_list[totalSockets] = peer_sock;
                totalSockets++;*/
                tracker_response.peers[i]->sock = peer_sock;
                tracker_response.peers[i]->finished_handshake = 0;
                tracker_response.peers[i]->am_choking = 1;
                tracker_response.peers[i]->am_interested = 0;
                tracker_response.peers[i]->peer_choking = 1;
                tracker_response.peers[i]->peer_interested = 0;

                tracker_response.peers[i]->last_keep_alive_time = time(NULL);
                tracker_response.peers[i]->total_bytes = bitfield_length+5+68;
                tracker_response.peers[i]->total_time = 0;
                tracker_response.peers[i]->last_keep_alive_time = time(NULL);
                tracker_response.peers[i]->bitfield = calloc(1, bitfield_length);
                initialize_bitfield(tracker_response.peers[i]->bitfield, bitfield_length);

                socket_list[totalSockets] = peer_sock;
                peers_list[totalSockets] = tracker_response.peers[i];
                send_client_handshake(peer_sock);
                send_bitfield(peer_sock);
                
                totalSockets++;

                // double test = ReadWatch(&readtime);
                // printf("%f\n", test);
            } else {
                // freeing peers
                if (tracker_response.peers[i]->peer_id != NULL) {
                    free(tracker_response.peers[i]->peer_id);
                }
                free(tracker_response.peers[i]);
                tracker_response.peers[i] = NULL;
            }
        }
    }
    fflush(stdout);
    curr_tracker_response = tracker_response;

    // ------------------------------UNCHOKE 4 PEERS WITH BEST BANDWIDTH-----------------------
    Peer **unchoke_list = malloc(4*8);
    int num_peers_unchoke;
    unchoke_peers_list(unchoke_list, &num_peers_unchoke);
    unchoke_choke_peers(unchoke_list, num_peers_unchoke);

    //----------------------------------- STARTING FOR LOOP -----------------------------------

    piece_list = calloc(num_pieces, sizeof(Peer));
    // time since we last sent out keep alive
    time_t last_keep_alive_time = time(NULL);
    // time since last interval time for tracker
    time_t last_tracker_interval_time = time(NULL);
    // time since we check who we choke and unchoke based on bandwidth
    time_t last_bandwidth_check_time = time(NULL);
    // time since we last requested piece
    time_t last_req_piece = time(NULL);
    // Specifiy the timeout for the receiving select() function
	struct timeval recv_timeout;
	recv_timeout.tv_sec = 0;
	recv_timeout.tv_usec = 0;
    
    // for every connection send the initial handshake message
    // for (int i = 1; i<totalSockets; i++) {
    //     peers_list[i-1]->start = time(NULL);
    // }
    last_piece_time = time(NULL);
    time_t last_reconnect = time(NULL);
    fd_set sock_set;
    for (;;) {

        // Setting sock descriptor for select()
        FD_ZERO(&sock_set);

        for (int i = 0; i <totalSockets; i++) {
            FD_SET(socket_list[i], &sock_set);
        }

        // Call select()
		if (select(FD_SETSIZE, &sock_set, NULL, NULL, &recv_timeout) > 0) {
            for (int i = 0; i< totalSockets; i++) {
                if (FD_ISSET(socket_list[i], &sock_set) != 0) {
                    //receiving on the client_socket- NOT TESTED
                    if (i == 0) {
                        accept_incoming();
                    //receiving from an existing peer
                    } else {
                        peer_message(i);

                    }

                }
            }
        }
        update_socket_list();

        //CHECK IF WE ARE A SEEDER YET
        if (downloaded_all == 0 && left <= 0) {
            downloaded_all = 1;
            combineallfiles(num_pieces, metafile_message.piece_length);
        }

        time_t curr_time = time(NULL);
        // set keep_alive messages to send every 10 seconds
        if (curr_time - last_keep_alive_time >= 10) {
            keep_alive();
            last_keep_alive_time = time(NULL);
            puts ("sent out keep alive message");
        }
        // Commented out keep_alive_check() so we don't close connections if the client is not sending us keep alive messages
        keep_alive_check();
        curr_time = time(NULL);
        if (curr_time-last_bandwidth_check_time >= 180) {
            // if 180 seconds gone by, recheck bandwidth
            // for (int j=0; j<totalSockets;j++) {
            //     send_choked(socket_list[j]);
            // }
            unchoke_peers_list(unchoke_list, &num_peers_unchoke);
            unchoke_choke_peers(unchoke_list, num_peers_unchoke);
        }
        fflush(stdout);

        // if an "interval" amount of time has passed by, send a request to the tracker to update information in tracker_response
        // curr_time = time(NULL);

        // if (curr_time-last_reconnect >= 20) {
        //     reconnect_peers();
        //     last_reconnect= time(NULL);
        // }
        curr_time=time(NULL);
        if (curr_time - last_tracker_interval_time >= tracker_response.interval || ((curr_time-last_piece_time >=30 || totalSockets == 1) && downloaded_all != 1)) {
            last_tracker_interval_time = time(NULL);
            puts ("sent out tracker request");
            tracker_req_interval();
            printf("now: %ld, last: %ld\n", curr_time, last_tracker_interval_time);
            last_piece_time = time(NULL);
        }


//----------------------------------- SENDING REQUESTS -----------------------------------
        // consistently check for every peer
        // (if we are interested and we are unchocked->
        // then get the bits that they have that we dont have an request it)
        curr_time = time(NULL);
        // set keep_alive messages to send every 10 seconds
        if (curr_time-last_req_piece>=0.5 && downloaded_all != 1) {
            last_req_piece = time(NULL);
            int indices[totalSockets];
            int requests = 0;
            for (int i = 0; i<50; i++) {
                if (peers_list[i] != NULL) {
                    if (peers_list[i]->am_interested == 1 || peers_list[i]->peer_choking == 0) {
                        int req_bits[num_pieces];
                        send_interested(peers_list[i]->sock);
                        int ret_length = compare_bitfields(bitfield, peers_list[i]->bitfield,req_bits, bitfield_length);
                        // printf("sending %d requests to socket %d\n", ret_length, peers_list[i]->sock);
                        // for (int j = 0; j<ret_length; j++) {
                        //     // NOTE: offset within the piece set to 0, NEED TO UPDATE BASED ON WHAT WE HAVE
                        //     // IS LAST PIECE DIFF LENGTH?
                        //     // printf("req_bits[j]: %d\n", req_bits[j]);
                        //     send_request(peers_list[i]->sock, req_bits[j], 0, metafile_message.piece_length);
                        // }

                        // send not interested to peer if we don't need any of their bits
                        if (ret_length == 0) {
                            peers_list[i]->am_interested = 0;
                            send_notinterested(peers_list[i]->sock);
                            continue;
                        }

                        // continute the request
                        int piece_size_request = metafile_message.piece_length > REQUEST_BYTE_CAP ? REQUEST_BYTE_CAP: metafile_message.piece_length;
                        int index_req = -1;
                        for (int j=0; j<ret_length;j++) {
                            // if (get_bit(bitfield, req_bits[j]) == 0 && (piece_list[req_bits[j]] == NULL  || piece_list[req_bits[j]]->isComplete == 0)) {
                            for (int k=0;k<totalSockets;k++) {
                                if (k==requests) {
                                    indices[requests++] = req_bits[j];
                                    index_req = j;
                                    break;
                                } else if (indices[k] == req_bits[j]) {
                                    break;
                                }
                            }
                            if (index_req != -1) {
                                break;
                            }
                            // }
                        }
                        
                        if (index_req == -1) {
                            continue;
                        } else if (req_bits[index_req] == num_pieces - 1) {
                            int new_len = metafile_message.total_length-metafile_message.piece_length*18;
                            piece_size_request = new_len > REQUEST_BYTE_CAP ? REQUEST_BYTE_CAP: new_len;
                            if (piece_list[req_bits[index_req]] != NULL) {
                                if (piece_size_request+piece_list[req_bits[index_req]]->begin > new_len) {
                                    piece_size_request = new_len-piece_list[req_bits[index_req]]->begin;
                                }
                                send_request(peers_list[i]->sock, req_bits[index_req], piece_list[req_bits[index_req]]->begin, piece_size_request);
                            } else {
                                if (piece_size_request > new_len) {
                                    piece_size_request = new_len;
                                }
                                send_request(peers_list[i]->sock, req_bits[index_req], 0,piece_size_request);
                            }
                        }
                        if (piece_list[req_bits[index_req]] != NULL) {
                            if (piece_size_request+piece_list[req_bits[index_req]]->begin > metafile_message.piece_length) {
                                piece_size_request = metafile_message.piece_length-piece_list[req_bits[index_req]]->begin;
                            }
                            send_request(peers_list[i]->sock, req_bits[index_req], piece_list[req_bits[index_req]]->begin, piece_size_request);
                        } else {
                            if (piece_size_request > metafile_message.piece_length) {
                                piece_size_request = metafile_message.piece_length;
                            }
                            send_request(peers_list[i]->sock, req_bits[index_req], 0,piece_size_request);
                        }
                    } 
                    // else if (){
                    //     // leechers not choking us
                    //     int zero_index = firstzero(bitfield, bitfield_length);
                    //     if (zero_index != -1) {
                    //         if (piece_list[zero_index] != NULL) {

                    //         } else {

                    //         }
                    //     }

                    // }
                }
            }
        }

        fflush(stdout);

    }
}


//----------------------------------- MAIN FUNCTIONS BELOW -----------------------------------

// unchoke based on bandwidth
void unchoke_peers_list(Peer ** list, int *num_peers) {
    int num = 0;
    for (int i =0; i<50;i++) {
        if (peers_list[i] != NULL && peers_list[i]->peer_interested == 1) {
            if (num < 4) {
                list[num++] = peers_list[i];
            } else {
                // readjust list to have highest bandwidth/lowest rtt peers
                Peer *curr = peers_list[i];
                for (int j =0; j<4;j++) {
                    if (compare_bandwidth(curr,list[j]) < 0) {
                        Peer *temp = list[j];
                        list[j] = curr;
                        curr = temp;
                    }
                }
            }
        }
    }

    if (num_peers) {
        *num_peers = num;
    }
}
// void unchoke_peers_list_with_optimistic(Peer ** list, int *num_peers) {
//     int num = 0;
//     int count = 0;
//     Peer *optimistic;
//     for (int i =0; i<50;i++) {
//         if (peers_list[i] != NULL && peers_list[i]->peer_interested == 1) {
//             count++;
//             if (num < 3) {
//                 list[num++] = peers_list[i];
//             } else {
//                 // readjust list to have 3 highest bandwidth/lowest rtt peers
//                 Peer *curr = peers_list[i];
//                 for (int j =0; j<3;j++) {
//                     if (compare_bandwidth(curr,list[j]) < 0) {
//                         Peer *temp = list[j];
//                         list[j] = curr;
//                         curr = temp;
//                     }

//                 }
//             }
//         } 
//     }
    // int rand = (rand() % (count – 3 + 1));
    // int counter2 = 0;
    // if(rand > 0){
    //     for (int i = 0; i<50;i++) {
    //         if (peers_list[i] != NULL && peers_list[i]->peer_interested == 1) {
    //             if(peers_list[i]!=list[0] &&peers_list[i]!=list[1] &&peers_list[i]!=list[2] ){
    //                 counter2++;
    //                 if(counter2 == rand){
    //                     list[3] = peerslist[i];
    //                     break;
    //                 }
    //             }
    //         }
    //     }
    //     num++;
    // }
// 
//     if (num_peers) {
//         *num_peers = num;
//     }
// }

void unchoke_choke_peers(Peer **unchoke_list, int num_peers) {
    for (int i=0;i<50;i++) {
        if (peers_list[i] != NULL) {
            uint8_t inList = 0;
            for (int j=0; j<num_peers; j++) {
                if (peers_list[i] == unchoke_list[j]) {
                    if (peers_list[i]->am_choking == 1) {
                        peers_list[i]->am_choking = 0;
                        send_notchoked(peers_list[i]->sock);
                    }
                    inList = 1;
                    break;
                }
            }

            // if the peer is not in the unchoke list and is unchoking peer
            // we choke the peer
            if (inList == 0 && peers_list[i]->am_choking == 0) {
                peers_list[i]->am_choking = 1;
                send_choked(peers_list[i]->sock);
            }

        }
    }
}

// returns -1 if p1 has better rtt than p2, 1 if p2 better than p1, 0 if same
int compare_bandwidth(Peer *p1, Peer *p2) {
    if (p1->total_bytes/p1->total_time > p2->total_bytes/p2->total_time) {
        return -1;
    } else if (p1->total_bytes/p1->total_time < p2->total_bytes/p2->total_time) {
        return 1;
    }
    return 0;
}

void accept_incoming() {
    // client address
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    // accept connection
    int new_sock = accept(socket_list[0], (struct sockaddr *) &clientAddr, &clientAddrLen);
    if (new_sock < 0) {
        puts("unsuccessfully waiting for client to connect");
        return;
    }
    printf("INITIAL HANDSHAKE MESSAGE FROM %d\n", new_sock);

    // receive the handshake_message
    HandshakeMessage handshake_message;
    if (receive_client_handshake(new_sock, &handshake_message) != 1) {
        puts("failed to receive client_handshake");
        close(new_sock);
        return;
    }

    // if the info_hash is the same as ours, then add the connection and call update_socket_list(), else drop the connection
    if (memcmp(handshake_message.info_hash, info_hash, 20) == 0) {
        add_peer_peerlist_finished_handshake(new_sock, clientAddr.sin_addr.s_addr , clientAddr.sin_port, handshake_message.peer_id);
        update_socket_list();

        //send a handshake message response
        send_client_handshake(new_sock);
        //send a bitfield if we have pieces
        send_bitfield(new_sock);
        printf("Received new peer %s\n", inet_ntoa(clientAddr.sin_addr));
    } else {
        // peer_id mismatch
        close(new_sock);
    }
}

void peer_message(int i) {
    //finds the peer with this socket
    Peer *curr_peer = find_peer_peerlist(socket_list[i]);

        //if the peer has not finished the handshake, receive the handshake back
    if (curr_peer->finished_handshake == 0) {

        HandshakeMessage handshake_message;
        if (receive_client_handshake(socket_list[i], &handshake_message) != 1) {
            // close socket and remove peer
            remove_sock_peerlist(socket_list[i]);
            return;
        }

        //update the peer ids or compare the peer ids depending on whether compact is true or not
        if (compact == 1) {
            curr_peer->peer_id = malloc(21);
            memcpy(curr_peer->peer_id, handshake_message.peer_id, 20);
            curr_peer->peer_id[20] = '\0';
        } else {
            // if the ids are not the same, drop the connection and update socket list
            if (memcmp(curr_peer->peer_id, handshake_message.peer_id, 20) != 0) {
                // close(socket_list[i]);
                //remove the socket from the socket list and remove the peer from the list
                remove_sock_peerlist(socket_list[i]);
                return;
            }
        }

        curr_peer->finished_handshake = 1;

    //receiving a message
    } else {
        //receive the first four bytes to detetermine which type of message we are receiving
        char length_type[4];
        if (recv_bytes(socket_list[i], length_type, 4) != 1){
            // peer has closed, remove peer and close socket
            remove_sock_peerlist(socket_list[i]);
            return;
        }
                        
        int length;
        memcpy(&length, length_type, 4);
        length = be32toh(length);
                        
        // got a keep-alive message; "keep-alive";
        if (length == 0) {
            //puts("GOT SENT KEEP-ALIVE MESSAGE");
            // TODO: might want to close connections that don't send keep-alive messages for a while
        } else {
            char get_type[1];
            recv_bytes(socket_list[i], get_type, 1);
            uint8_t type = get_type[0];

            // printf("Socket %d ", socket_list[i]);
            // got a notification that the peer choking me; "choke"
            if (type == 0) {
                printf("GOT SENT CHOKE FROM SOCK %d\n", socket_list[i]);
                curr_peer->peer_choking = 1;

            // got a notification that the peer is not choking me; "unchoke"
            } else if (type == 1) {
                printf("GOT SENT UNCHOKE FROM SOCK %d\n", socket_list[i]);
                curr_peer->peer_choking = 0;

            // got a notification that the peer is interested in me; "interested"
            } else if (type == 2) {
                printf("GOT SENT INTERESTED FROM SOCK %d\n", socket_list[i]);
                curr_peer->peer_interested = 1;
                            
            // got a notification that the peer is not interested in me; "not interested"
            } else if (type == 3) {
                printf("GOT SENT NOTINTERESTED FROM SOCK %d\n", socket_list[i]);
                curr_peer->peer_interested = 0;
                            
            // got a notification that the peer has a piece; "have"
            } else if (type == 4) {
                printf("GOT SENT A HAVE FROM SOCK %d\n", socket_list[i]);
                char buffer[length-1];
                recv_bytes(socket_list[i], buffer, length-1);

                int piece_index = be32toh(*((int *)buffer));
                set_bit(curr_peer->bitfield, piece_index);
                if (find_first_one_bit(bitfield, curr_peer->bitfield, bitfield_length) != -1) {
                    send_interested(socket_list[i]);
                }
            // got a bitfield; "bitfield"
            } else if (type == 5) {
                printf("GOT SENT A BITFIELD FROM SOCK %d\n", socket_list[i]);

                //if sent a wrong bitfield length, we close the connection
                if (length-1 != bitfield_length) {
                    remove_sock_peerlist(socket_list[i]);
                } else {
                    recv_bytes(socket_list[i], curr_peer->bitfield, bitfield_length);

                    //if any spare bits are set, we close the connection
                    if (compare_spare_bits(curr_peer->bitfield, bitfield_length, num_pieces) == -1) {
                        remove_sock_peerlist(socket_list[i]);
                    } else {
                        int bitfield_pos = find_first_one_bit (bitfield, curr_peer->bitfield, bitfield_length);
                        printf("we need their bitfield %d\n", bitfield_pos);
                        // i am interested in this peer and I send a message to the peer saying im interested
                        if (bitfield_pos != -1) {
                            curr_peer->am_interested = 1;
                            send_interested(socket_list[i]);
                        }
                        //prints the bitfield
                        //fwrite(curr_peer->bitfield, 1, bitfield_length, stdout);
                    } 
                }

            // got a request; "request"
            } else if (type == 6) {
                printf("GOT SENT A REQUEST FROM SOCK %d\n", socket_list[i]);
                char buffer[length-1];
                recv_bytes(socket_list[i], buffer, length-1);

                if (curr_peer->am_choking == 0) {
                    int index = be32toh(*((int *)buffer));
                    int begin = be32toh(*((int *)(buffer+4)));
                    length = be32toh(*((int *)(buffer+8)));

                    Piece *piece = piece_list[index];

                    if (piece != NULL && piece->begin > begin+length) {
                        // if we have the piece then read it
                        uint8_t *block = malloc(length);
                        if (readFile(block, begin, length, index) == 0) {
                            puts("READ FILE ERROR");
                            free(block);
                            return;
                        }
                        send_piece(socket_list[i], index, begin, block, length);
                        uploaded += length;
                        free(block);
                    }
                }
            // got the piece; "piece"
            } else if (type == 7) {
                printf("GOT SENT A PIECE FROM SOCK %d\n", socket_list[i]);
                last_piece_time = time(NULL);
                uint8_t buffer[8];
                recv_bytes(socket_list[i], buffer, 8);
                int index = be32toh(*((int *)buffer));
                int begin = be32toh(*((int *)(buffer+4)));
                length -= 9;  // length now refers to length of block
                // printf("Size of block %d\n", length);
                uint8_t *block = malloc(length);
                // need to receive bytes across segments

                // t
                time_t last_block_time = time(NULL);
                int total = 0;
                struct stopwatch sw;
                StartWatch(&sw);
                while (total < length) {
                    ssize_t numRecv = recv(socket_list[i], block+total, length-total, SOCK_NONBLOCK);
                    if (numRecv < 0) {
                        if (time(NULL)-last_block_time > 5) {
                            remove_sock_peerlist(socket_list[i]);
                            return;
                        }
                        perror("");
                        // return;
                    } else {
                        total += numRecv;
                    }
                    
                }
                // recv(socket_list[i], block, length, 0);
                addtime(&sw, *curr_peer);
                curr_peer->total_bytes += 8+length;
                fflush(stdout);
                // we throw away the block if we have completed the piece
                // or we already have the block for our partial piece
                if (get_bit(bitfield, index) == 0) {
                    // we need the piece, check if piece object exists
                    if (piece_list[index] == NULL && begin == 0) {
                        // puts("flag");
                        piece_list[index] = malloc(sizeof(Piece));
                        
                        char *name = createfilename(index);
                        // puts("flag");
                        piece_list[index]->fp = createfile(block, length, name);
                        piece_list[index]->begin = (uint32_t) length; // begin refers to next byte we want
                        free(name);
                    // taking a part of the block due to overlapping
                    } else if (piece_list[index]->begin >= begin && piece_list[index]->begin < begin+length){

                        fwrite(block, 1, length, piece_list[index]->fp);

                        piece_list[index]->begin += length;
                    } else {
                        puts("Discarding piece");
                    }
                    
                    // printf("Next byte %d : Piece Length %ld\n", piece_list[index]->begin, metafile_message.piece_length);
                    // check if piece is complete aka if begin == piece_length
                    if (index == num_pieces-1) {
                        if (metafile_message.total_length-metafile_message.piece_length*18 == piece_list[index]->begin) {
                            piece_list[index]->isComplete = 1;
                        }
                    }
                    if (piece_list[index]->begin == metafile_message.piece_length || piece_list[index]->isComplete == 1) {
                        piece_list[index]->isComplete = 1;

                        // compare the hashes, if it returns -1 then set "isComplete" to 0 and set begin to 0
                        if (compare_piece_hash(piece_list[index], index) == -1) {
                            puts("THE HASHES ARE NOT THE SAME------------------------");
                            piece_list[index]->isComplete = 0;
                            piece_list[index]->begin = 0;

                        } else {
                            printf("Verified piece %d\n", index);
                            fclose(piece_list[index]->fp);
                            set_bit(bitfield, index);

                            //update downloaded/left
                            if (index == num_pieces-1) {
                                downloaded += piece_list[index]->begin;
                                left = 0;
                            } else {
                                downloaded += metafile_message.piece_length;
                                left -= metafile_message.piece_length;
                            }
                            

                            // send HAVE to peers that don't have the piece
                            for (int j = 1; j<totalSockets;j++) {
                                if (get_bit(curr_peer->bitfield, index) != 1) {
                                    send_have(socket_list[j], index);
                                    // printf("sent have to socket %d\n", socket_list[j]);
                                }
                            }

                        }
                        
                    }
                    

                }
                free(block);
                            
            // peer cancelled block request; "cancel"
            } else if (type == 8) {
                printf("GOT SENT CANCEL FROM SOCK %d\n", socket_list[i]);
                char buffer[length-1];
                recv_bytes(socket_list[i], buffer, length-1);
            } else {
                printf("Unknown Type: %02X send by sock %d\n", type, socket_list[i]);
            }

        }

    }
    curr_peer->last_keep_alive_time = time(NULL);
}

void tracker_req_interval() {
    // stores the response as an arbitrary size
    int response_size = 4096;
    char response[response_size];
    tracker_message.left = left;
    tracker_message.uploaded = uploaded;
    tracker_message.downloaded = downloaded;

    // if we are not a seeder
    if (downloaded_all == 1) {
        strncpy(tracker_message.event, "completed", 10);
        tracker_message.event[9] = '\0';
    }

    // send request to tracker
    while (send_get_req(host, slash_URL, tracker_ip, tracker_port, &tracker_message, response, response_size) == 0) {
        puts("failed to send request to tracker");
        // exit(1);
    }
   
    //initializes the tracker_response, bencodes and sets the appropriate fields of the tracker_response
    TrackerResponse new_tracker_response;
    bencode_tracker_response(response, response_size, &new_tracker_response);
    if (new_tracker_response.peers_length <= 0) {
        //tracker response failed to give us any peers
        return;
    }
    //update the list of peers
    //if the peer is not in our peer list, then make a connection and send initial handshake message
    for (int i = 0; i<50; i++) {
        int found = 0;
        for (int j = 0; j<50; j++) {
            if (peers_list[j] != NULL && new_tracker_response.peers[i] != NULL) {
                if (new_tracker_response.peers[i]->ip == peers_list[j]->ip && new_tracker_response.peers[i]->port == peers_list[j]->port) {
                    found = 1;
                    if (new_tracker_response.peers[i]->peer_id != NULL) {
                        free(new_tracker_response.peers[i]->peer_id);
                    } 

                    free(new_tracker_response.peers[i]);
                    new_tracker_response.peers[i] = peers_list[j];
                }
            }
        }
        if (found == 0 && new_tracker_response.peers[i] != NULL && new_tracker_response.peers[i]->port != 0) {
            int peer_sock = connect_to_client(new_tracker_response.peers[i]->ip, new_tracker_response.peers[i]->port);
            if (peer_sock != -1) {
                int added_peer = 0;
                for (int k = 0; k <50; k++) {
                    if (peers_list[k] == NULL) {
                        new_tracker_response.peers[i]->sock = peer_sock;
                        new_tracker_response.peers[i]->finished_handshake = 0;
                        new_tracker_response.peers[i]->am_choking = 1;
                        new_tracker_response.peers[i]->am_interested = 0;
                        new_tracker_response.peers[i]->peer_choking = 1;
                        new_tracker_response.peers[i]->peer_interested = 0;
                        new_tracker_response.peers[i]->last_keep_alive_time = time(NULL);
                        new_tracker_response.peers[i]->bitfield = malloc(bitfield_length);
                        initialize_bitfield(new_tracker_response.peers[i]->bitfield, bitfield_length);
                        peers_list[k] = new_tracker_response.peers[i];
                        
                        send_client_handshake(peer_sock);
                        send_bitfield(peer_sock);
                        added_peer = 1;
                        break;
                    }
                }
                // if (added_peer == 0) {
                //     // freeing peers
                //     if (new_tracker_response.peers[i]->peer_id != NULL) {
                //         free(new_tracker_response.peers[i]->peer_id);
                //     }
                //     free(new_tracker_response.peers[i]);
                //     new_tracker_response.peers[i] = NULL;
                // }
            } else {
                if (new_tracker_response.peers[i]->peer_id != NULL) {
                    free(new_tracker_response.peers[i]->peer_id);
                }
                free(new_tracker_response.peers[i]);
                new_tracker_response.peers[i] = NULL;
            }
        }
    }
    curr_tracker_response = new_tracker_response;
    update_socket_list();
}


//----------------------------------- HELPER FUNCTIONS -----------------------------------

// attempts to reconnect to peers in peer list from tracker
void reconnect_peers() {
    for (int i=0;i<50;i++) {
        if (curr_tracker_response.peers[i] != NULL) {
            if (find_peer_peerlist(curr_tracker_response.peers[i]->sock) != NULL) {
                continue;
            }
        }
        if (curr_tracker_response.peers[i] != NULL && curr_tracker_response.peers[i]->port != 0) {
            int peer_sock = connect_to_client(curr_tracker_response.peers[i]->ip, curr_tracker_response.peers[i]->port);
            if (peer_sock != -1) {
                int added_peer = 0;
                for (int k = 0; k <50; k++) {
                    if (peers_list[k] == NULL) {
                        printf("Reconnecting peer to socket %d\n", peer_sock);
                        curr_tracker_response.peers[i]->sock = peer_sock;
                        curr_tracker_response.peers[i]->finished_handshake = 0;
                        curr_tracker_response.peers[i]->am_choking = 1;
                        curr_tracker_response.peers[i]->am_interested = 0;
                        curr_tracker_response.peers[i]->peer_choking = 1;
                        curr_tracker_response.peers[i]->peer_interested = 0;
                        curr_tracker_response.peers[i]->last_keep_alive_time = time(NULL);
                        curr_tracker_response.peers[i]->bitfield = malloc(bitfield_length);
                        initialize_bitfield(curr_tracker_response.peers[i]->bitfield, bitfield_length);
                        peers_list[k] = curr_tracker_response.peers[i];
                        
                        send_client_handshake(peer_sock);
                        send_bitfield(peer_sock);
                        added_peer = 1;
                        break;
                    }
                }
                // if (added_peer == 0) {
                //     // freeing peers
                //     if (new_tracker_response.peers[i]->peer_id != NULL) {
                //         free(new_tracker_response.peers[i]->peer_id);
                //     }
                //     free(new_tracker_response.peers[i]);
                //     new_tracker_response.peers[i] = NULL;
                // }
            }
        }
    }
    update_socket_list();
}

void keep_alive_check() {
    time_t curr_time = time(NULL);
    for (int i =0;i<50;i++) {
        if (peers_list[i] != NULL) {
            if (curr_time - peers_list[i]->last_keep_alive_time > 100) {
                remove_sock_peerlist(peers_list[i]->sock);
                totalSockets--;
            }
        }
    }
}

// find the peer for a given sock 
Peer *find_peer_peerlist(int sock) {
    for (int i = 0; i<50; i++) {
        if (peers_list[i] != NULL) {
            if (peers_list[i]->sock == sock) {
                return peers_list[i];
            }
        }
    }
    return NULL;
}

// removes a peer from the peer list based on the socket
void remove_sock_peerlist(int sock) {
    for (int i = 0; i<50; i++) {
        if (peers_list[i] != NULL) {
            if (peers_list[i]->sock == sock) {
                printf("Removed socket %d from socket list\n", sock);
                close(peers_list[i]->sock);
                if (peers_list[i]->peer_id != NULL) {
                    free(peers_list[i]->peer_id);
                }
                free(peers_list[i]->bitfield);
                free(peers_list[i]);
                peers_list[i] = NULL;
                update_socket_list();
                return;
            }
        }
    }
}

// add a peer to the peer list after finishing handshake
void add_peer_peerlist_finished_handshake(int sock, uint16_t ip, uint32_t port, char *peer_id) {
    for (int i = 0; i<50; i++) {
        if (peers_list[i] == NULL) {
            Peer *new_peer = malloc(sizeof(Peer));
            new_peer->finished_handshake = 1;
            new_peer->am_choking = 1;
            new_peer->am_interested = 0;
            new_peer->peer_choking = 1;
            new_peer->peer_interested = 0;
            new_peer->ip = ip;
            new_peer->port = port;
            new_peer->sock = sock;
            if (peer_id != NULL) {
                new_peer->peer_id = malloc(21);
                strncpy(new_peer->peer_id, peer_id, 21);
                new_peer->peer_id[20] = '\0';
            } else {
                new_peer->peer_id = NULL;
            }
            new_peer->last_keep_alive_time = time(NULL);
            new_peer->bitfield = calloc(1, bitfield_length);
            initialize_bitfield(new_peer->bitfield, bitfield_length);
            peers_list[i] = new_peer;
            return;
        }
    }
}

// updates the socket list based on the peer list
void update_socket_list() {
    int count_sock = 1;
    for (int i = 0; i<50; i++) {
        if (peers_list[i] != NULL) {
            socket_list[count_sock++] = peers_list[i]->sock;
        }
    }
    totalSockets = count_sock;
    
}


//implement bitfield as array of uint8_t
// uint8_t* bitfield;
// void update_bitfield(int position, int new_bit){
//     if(new_bit != 0 || new_bit != 1){
//         return;
//     }
//     int i = position/8;
//     int r = position%8;

//     uint8_t shift = new_bit;
//     shift = shift << r;
//     bitfield[i] = bitfield[i] | shift;
// }

// int* bits[8];
// void convert_to_bits(uint8_t* bitfield, int position, int* bits){
//     uint8_t temp = bitfield[position];
//     if(temp - pow(2,7) > 0){
//         bits[0] = 1;
//         temp = temp - pow(2,7);
//     }
//     if(temp - pow(2,6) > 0){
//         bits[1] = 1;
//         temp = temp - pow(2,6);
//     }
//     if(temp - pow(2,5) > 0){
//         bits[2] = 1;
//         temp = temp - pow(2,5);
//     }
//     if(temp - pow(2,4) > 0){
//         bits[3] = 1;
//         temp = temp - pow(2,4);
//     }
//     if(temp - pow(2,3) > 0){
//         bits[4] = 1;
//         temp = temp - pow(2,3);
//     }
//     if(temp - pow(2,2) > 0){
//         bits[5] = 1;
//         temp = temp - pow(2,2);
//     }
//     if(temp - pow(2,1) > 0){
//         bits[6] = 1;
//         temp = temp - pow(2,1);
//     }
//     if(temp - pow(2,0) > 0){
//         bits[7] = 1;
//         temp = temp - pow(2,0);
//     }
// }

// int returnbit(uint8_t* bitfield, int position){
//     int i = position/8;
//     int r = position%8;
//     int* bits[8];
//     convert_to_bits(bitfield,i,bits);
//     return bits[r];
// }
