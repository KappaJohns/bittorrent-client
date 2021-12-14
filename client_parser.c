#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <argp.h>
#include "client_parser.h"

error_t client_parser(int key, char *arg, struct argp_state *state) {
	struct client_arguments *args = state->input;
	error_t ret = 0;
	int len;
	switch(key) {
	case 'a':
		len = strlen(arg);
		args->ip_address = malloc(len+1);
		strcpy(args->ip_address, arg);
		break;
	case 'p':
		/* Validate that port is correct and a number, etc!! */
		args->port = atoi(arg);
		if (0 /* port is invalid */) {
			argp_error(state, "Invalid option for a port, must be a number");
		}
		break;
	case 'f':
		/* validate file */
		len = strlen(arg);
		args->filename = malloc(len + 1);
		strcpy(args->filename, arg);
		break;
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}
	return ret;
}

struct client_arguments client_parseopt(int argc, char *argv[]) {
	struct argp_option options[] = {
		{ "addr", 'a', "addr", 0, "The IP address the Chord client binds to", 0},
		{ "port", 'p', "port", 0, "The port that is being used at the client", 0},
		{ "file", 'f', "file", 0, "The file that the client reads data from for all hash requests", 0},
		{0}
	};

	struct argp argp_settings = { options, client_parser, 0, 0, 0, 0, 0 };

	struct client_arguments args;
	bzero(&args, sizeof(args));

	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0) {
		printf("Got error in parse\n");
	}

	/* If they don't pass in all required settings, you should detect
	 * this and return a non-zero value from main */
	printf("Got port %d and address %s and filename %s\n", args.port, args.ip_address, args.filename);
		  

	return args;
}
