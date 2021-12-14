#ifndef CLIENT_PARSER_H
#define CLIENT_PARSER_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <argp.h>

struct client_arguments {
	char *ip_address;
	char *filename;
	int port;
};

error_t client_parser(int key, char *arg, struct argp_state *state);

struct client_arguments client_parseopt(int argc, char *argv[]);

#endif