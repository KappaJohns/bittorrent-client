#ifndef STOPWATCH_H_
#define STOPWATCH_H_
#include<time.h>

#include <stdio.h>
#include <stdlib.h>
#include "stopwatch.h"
#include "client_parser.h"
#include "bencode.h"
#include "client.h"
#include "client_bencoding.h"
#include "bigint.h"
#include "hash.h"
#include "send_receive.h"
#include "bitfield.h"
#include "peer_message.h"
struct stopwatch{
	struct timespec ts;
	struct timespec unixx;
};
double _timespec2seconds(struct timespec *t);
void StartWatch(struct stopwatch *sw);
double ReadWatch (struct stopwatch *sw);
void addtime(struct stopwatch *sw, Peer peer);
// units = seconds
#endif/* STOPWATCH_H_ */