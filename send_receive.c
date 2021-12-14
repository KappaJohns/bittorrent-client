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
#include "bigint.h"
#include "stopwatch.h"

int send_bytes (int sendSock, void *sendType, size_t sendBytes) {
    Peer * peer = find_peer_peerlist(sendSock);
    struct stopwatch sw;
    StartWatch(&sw);
    size_t difference = 0;
    while (difference < sendBytes) {
        ssize_t numSent = send(sendSock, sendType + difference, sendBytes - difference, 0);
        if (numSent <0) {
            printf("Send error socket %d\n", sendSock);
            perror("");
            return -1;
        }
        difference+= numSent;
    }
    if (peer != NULL) {
        addtime(&sw, *peer);
        peer->total_bytes += sendBytes;
    }
    // puts("Send complete");
    return 1;
}

int recv_bytes (int recvSock, void *recvType, size_t recvBytes) {
    Peer * peer = find_peer_peerlist(recvSock);
    struct stopwatch sw;
    StartWatch(&sw);
    size_t difference = 0;
    while (difference < recvBytes) {
        ssize_t numRecv = recv(recvSock, recvType+difference, recvBytes-difference, SOCK_NONBLOCK);
        if (numRecv <0) {
            perror("Recv error");
            return -1;
        } else if (numRecv == 0) {
            return 0;
        }
        difference +=numRecv;
    }
    if (peer != NULL) {
        addtime(&sw, *peer);
        peer->total_bytes += recvBytes;
    }
    
    return 1;
}

/* splits the announce into the slash_URL and host for the tracker get request */
void split_announce(char *announce, int length, char *host, char *slash_URL) {

    char *temp = announce;
    int start_host = 0;
    int start_slash_URL = 0;
    int index_host = 0;
    int index_slash_URL = 0;
    for (int i = 1; i<length; i++) {
        if (temp[i-1] == '/' && temp[i] == '/') {
            start_host = 1;
            continue;
        } 
        if (start_host == 1) {
            if (temp[i] == '/') {
                start_host = 0;
                start_slash_URL = 1;
            } else {
                host[index_host++] = temp[i];
            }
        }
        if (start_slash_URL == 1) {
            slash_URL[index_slash_URL++] = temp[i];
        }
    }
    host[index_host] = '\0';
    slash_URL[index_slash_URL] = '\0';
}

/* send get request to tracker */
int send_get_req(char *host, char *slash_URL, char *ip, int port, TrackerMessage *message, char *response, int response_size) {

    /* creates a socket using TCP */
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        puts("creating a socket failed");
        return 0;
    }

    /* get the server ip */
    struct hostent *server;
    server = gethostbyname(ip);
    if (server == NULL) {
        puts ("failed to get host by name");
        return 0;
    }

    /* construct server address structure */
    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    memcpy(&servAddr.sin_addr.s_addr,server->h_addr_list[0],server->h_length);
    servAddr.sin_port = htons(port);
    
    /* connect to the socket */
    while (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        // puts ("failed to connect to the server");
        // return 0;
        sleep(3);
    }

    //Print statement to check the tracker IP address and port
    printf("IP address: %s and port %d\n", inet_ntoa(*(struct in_addr*)server->h_addr_list[0]), port);
    
    /* send the get request to the tracker */
    char request [2000];
    snprintf(request, 2000,
		 "GET %s"
         "?info_hash=%s&"
		 "peer_id=%s&"
		 "port=%d&"
		 "uploaded=%ld&"
		 "downloaded=%ld&"
		 "left=%ld&"
         "numwant=50&"
         "compact=1&"
		 "event=%s&"
         "ip=%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\n\r\n",
		 slash_URL, message->info_hash, message->peer_id, message->port, message->uploaded, message->downloaded, message->left, message->event, this_host_ip, host);
    int total = 2000;
    int sent = 0;
    int bytes = 0;
    while (sent < total) {
        bytes = write(sock, &request[sent], total-sent);
        if (bytes < 0) {
            puts ("failed get request");
            perror("");
            return 0;
        }
        sent+=bytes;
    }
    // Prints the request
    printf("\nGet Request:\n");
    printf("%s\n", request);
    // receive the response from the tracker
    total = response_size;
    sent = 0;
    int received = 0;
    while (received < total) {
        bytes = read(sock, &response[received], total-sent);
        if (bytes < 0) {
            puts ("failed get request");
            return 0;
        }
        if (bytes == 0) {
            break;
        }
        received+=total;
    }
    response[received] = '\0';

    //prints the response
    // printf("\nResponse: %s\n", response);

    close(sock);
    return 1;
}


// referencing: https://gist.github.com/jesobreira/4ba48d1699b7527a4a514bfa1d70f61a
char* urlencode(unsigned char* originalText)
{
    // allocate memory for the worst possible case (all characters need to be encoded)
    char *encodedText = (char *)malloc(sizeof(char)*strlen((char *)originalText)*3+1);
    
    const char *hex = "0123456789abcdef";
    
    int pos = 0;
    for (int i = 0; i < (int) strlen((char *)originalText); i++) {
        if (('a' <= originalText[i] && originalText[i] <= 'z')
            || ('A' <= originalText[i] && originalText[i] <= 'Z')
            || ('0' <= originalText[i] && originalText[i] <= '9')) {
                encodedText[pos++] = originalText[i];
            } else {
                encodedText[pos++] = '%';
                encodedText[pos++] = hex[originalText[i] >> 4];
                encodedText[pos++] = hex[originalText[i] & 15];
            }
    }
    encodedText[pos] = '\0';
    return encodedText;
}

// gets the socket bound to this client's port
int get_my_sock(int port) {

    /* creates socket for incoming connections */
    int servSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (servSock < 0) {
        puts("unsuccessful socket created for incoming connections");
        return -1;
    }

    /* create local address structure */
    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);

    /* bind socket to local address */
    if (bind(servSock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        puts("socket not bound to local address");
        return -1;
    }
    /* listens for incoming connections */
    if (listen(servSock, 10) < 0) {
        puts("unsuccessfully listening for incoming connections");
        return -1;
    }

    return servSock;
}

// connect to another client/peer through 3-way handshake
// initialized handshake message
int connect_to_client(uint32_t ip, uint16_t port) {

    /* creates a socket using TCP */
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    //int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    if (sock < 0) {
        puts("creating a socket failed");
        return -1;
    }

    /* construct server address structure */
    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;

    /* convert string representation of address to 32-bit binary represenation */
    servAddr.sin_addr.s_addr = ip;
    servAddr.sin_port = port;

    // if the the ip arg is true
    if (have_ip_arg == 1 && strcmp(ip_arg, inet_ntoa(servAddr.sin_addr)) == 0) {
        inet_pton(AF_INET, this_host_ip, &(servAddr.sin_addr));
    }

    // close the sock if we are connecting to ourself
    if (strcmp(inet_ntoa(servAddr.sin_addr), this_host_ip) == 0 && be16toh(servAddr.sin_port) == port_arg) {
        puts("Closed the connection with myself");
        close(sock);
        return -1;
    }

    //print binary address and port
    printf("ip %s, port %d:\n", inet_ntoa(servAddr.sin_addr), be16toh(servAddr.sin_port));

    // half a second
    struct timeval timer;
	timer.tv_sec  = 0;  
	timer.tv_usec = 500000; 
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timer, sizeof(timer));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timer, sizeof(timer));


    /* connect to the server */
    if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        printf("failed to connect to the client %s\n", inet_ntoa(servAddr.sin_addr));
        return -1;
    }

    // print statement to see the connection
    printf("MADE THE CONNECTION ON SOCK: %d\n", sock);
    return sock;
}

//receives a handshake message, and stores into "handshake_message" that was passed into the function
int receive_client_handshake(int sock, HandshakeMessage *handshake_message) {  
    char buffer[68];
    if (recv_bytes(sock, buffer, 68)<0) {
        puts("failed to receive handshake from client2");
        return -1;
    }

    // get the handshake message received
    memcpy(&handshake_message->pstrlen, buffer, 1);
    handshake_message->pstr = malloc(19);
    if (handshake_message->pstrlen != 19) {
        return -1;
    }
    memcpy(handshake_message->pstr, buffer+1, handshake_message->pstrlen);
    char temp_pstr[] = "BitTorrent protocol";
    if (memcmp(handshake_message->pstr, temp_pstr, 19) != 0) {
        return -1;
    }
    memcpy(&handshake_message->reserved, buffer+1+19, 8);
    handshake_message->reserved = be64toh(handshake_message->reserved);
    handshake_message->info_hash = malloc(20);
    memcpy(handshake_message->info_hash, buffer+1+19+8, 20);
    handshake_message->peer_id = malloc(20);
    memcpy(handshake_message->peer_id, buffer+1+19+8+20, 20);


    printf("RECEIVED MESSAGE FROM SOCKET %d\n", sock);
    //printf("pstrlen %d\n", handshake_message->pstrlen);
    //printf("pstr %s\n", handshake_message->pstr);
    //printf("reserved %ld\n", handshake_message->reserved);
    //printf("info hash %s\n", handshake_message->info_hash);
    //printf("peer id %s\n", handshake_message->peer_id);

    return 1;
}

//send the first handshake message
int send_client_handshake(int sock) {

    uint8_t pstr = 19;
    uint64_t reserved = htobe64(0);  
    // build a buffer to send
    char buffer[68];
    memcpy(buffer, &pstr, 1);
    memcpy(buffer+1, "BitTorrent protocol", pstr);
    memcpy(buffer+1+19,&reserved, 8);
    memcpy(buffer+1+19+8, info_hash, 20);
    memcpy(buffer+1+19+8+20, peer_id, 20);

    //sends the handshake message
    if (send_bytes(sock, buffer, 68) == -1) {
        printf("failed to send handshake to client on socket %d\n", sock);
        return -1;
    }
    
    return 1;
}






// char* dirname = "downloads";
// int createdir(){
//     int check;
//     check = mkdir(dirname,0777);

//     if (!check){
//         printf("Directory created\n");
//         return 1;
//     }   
//     else {
//         printf("Unable to create directory\n");
//         return -1;
//     }

// }



// int createfile(uint8_t *data, int data_length, char* name){
// 	int check;
//     	char *dirname, *filename, *fulldir;
//     	FILE *fp;
	
// 	dirname=malloc(SIZE*sizeof(char));
//     	filename=malloc(SIZE*sizeof(char));
//     	fulldir=malloc(SIZE*sizeof(char));

// 	strcpy(filename, name);

//     	strcpy(fulldir, dirname);
//     	strcat(fulldir, "/");
//     	strcat(fulldir, filename);

//     	if (fp = fopen(fulldir, "w") != NULL ) {
//         	printf("File created\n");
//             fwrite(data, 1, datalength, fulldir);
// 		//write to file using fwrite, fprintf or fputs depending on how we want to store it
//         	fclose(fp);
//     	} else {
//         	printf("Unable to create file\n");
//     	}

//     // End function
//     return 0;
// }