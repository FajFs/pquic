#ifndef CODEL_H
#define CODEL_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h> 
#include <stdbool.h>

#ifndef MAX_MTU
#define MAX_MTU 1400
#endif

//structures to maintain Codel queues
typedef struct st_codel_params {
	uint64_t	target;
	uint64_t	ceThreshold;
	uint64_t	interval;
	uint32_t	mtu;
	bool		ecn;
}codel_params_t;

//follwoing IETF recommendation
//Defining structure to handle CoDel variables
typedef struct st_codel_vars{
    uint32_t    count;             //number of dropped packets
    uint32_t    last_count;         //count entry to dropping state
    bool        dropping;          //currently dropping?
    uint16_t    rec_inv_sqrt;        //reciprocal sqrt computaion
    uint64_t    first_above_time;    //when delay above target
    uint64_t    drop_next;         //next time to drop
    uint64_t    lDelay;            //sjourn time of last dequeued packet
}codel_vars_t;

typedef struct st_codel_stats {
	uint32_t		max_packet;
	uint32_t		drop_count;
	uint32_t		drop_len;
	uint32_t		ecn_mark;
	uint32_t		ce_mark;
}codel_stats_t;



// static void init_codel_params(codel_params_t *params){
//     //TODO: fix correct parameters
//     params->interval = 1;
//     params->target = 1;
//     params->ceThreshold = 1;
//     params->ecn = false; 
// }

// static void init_codel_stats(codel_stats_t *stats){
//     //TODO: fix correct parameters
//     stats->max_packet = 0;
// }

// static void init_codel_vars(codel_vars_t *vars){
//     memset(vars, 0, sizeof(*vars));
// }



#endif