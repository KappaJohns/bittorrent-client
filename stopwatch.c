
#include <stdio.h>
#include <time.h>
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

//    struct stopwatch readtime;
//     StartWatch(&readtime);
//      ReadWatch(&readtime)
void StartWatch(struct stopwatch *sw){
	clock_gettime(CLOCK_MONOTONIC, &sw->ts);
	sw->unixx = sw->ts;
	sw->ts.tv_sec = 0;
}

double ReadWatch(struct stopwatch *sw){
	double diff;
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	now.tv_sec -= sw->unixx.tv_sec;

	diff =_timespec2seconds(&now)-
		_timespec2seconds(&sw->ts);
	return diff;
}

/*
 * tv_sec = seconds(long long)
 * tv_nsec = nanoseconds(long)
 * clkID = CLOCK_MONOTONIC,
 */
#define NANO    1.0e-9
//////////////////////////////////////////////////////
//// Convert the linuxtimespec to a double.
///    The units are in seconds.
//
double _timespec2seconds(struct timespec *t){
	double dt;
	dt = (double)(t->tv_nsec)*NANO;
	dt += (double)(t->tv_sec);
	return dt;
}

void addtime(struct stopwatch *sw, Peer peer){
    // if(peer.total_time == NULL){
    //     peer.total_time = 0;
    // }
    peer.total_time += ReadWatch(sw);
}