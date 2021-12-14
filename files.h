#ifndef FILES_H
#define FILES_H
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
#include <dirent.h>
#include "client.h"
#include "bigint.h"

int createdir();
FILE *createfile(uint8_t *data, int data_length, char* name);
int checkexistance(char* name);
int checkcomplete(int i);
int combineallfiles(int i,int sizeofeach);
char *createfilename(int index);
int readFile(uint8_t *buffer, int begin, int length, int index);
int getbitfieldfromfiles(uint8_t *bitfield, int n, long int size);
#endif