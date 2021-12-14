#ifndef CLIENT_BENCODING_H
#define CLIENT_BENCODING_H

#include <stdint.h>
#include <stddef.h>
#include "client.h"

void bencode_metafile(char *file_buf, int char_index, MetaFile *metafile_message);
void bencode_tracker_response(char *file_buf, int size, TrackerResponse *tracker_response);
void assign_compact_peers(char *compact, int size, TrackerResponse *tracker_response);
int get_tracker_port (char *announce, int announce_length);

#endif