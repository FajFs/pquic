#ifndef FQCODEL_H
#define FQCODEL_H

#include "picoquic.h"
#include "coDel.h"

#include <linux/ip.h>
#include <stdio.h>


#include "list.h"
#include "jenkinsHash.h"


#ifndef IP_HEADER_PARAMS
#define IP_HEADER_PARAMS 5
#endif

#ifndef NUM_FLOWS
#define NUM_FLOWS 8
#endif

#ifndef MAX_MTU
#define MAX_MTU   1403
#endif
//Structures in this implementation will mimic the API available in pQUIC 


typedef struct st_codel_frame{
    uint64_t enqueue_time;
    reserve_frames_block_t *block;
}codel_frame_t;

//following the IETF specification
//Defining the structure to handle flows
typedef struct st_fqcodel_flow{
    queue_t             *codel_queue;
    struct list_head    flow_chain;
    int32_t             credits;      //current number of queue credits         
    uint32_t            dropped;      //# of drops or ECN marks on flow
    codel_vars_t        codel_vars; 
}fqcodel_flow_t;


//following the IETF specification omitting external clissification
typedef struct st_fqcodel_schedule_data{
    fqcodel_flow_t          *flows;
    uint32_t                *backlogs;
    uint32_t                flows_count;
    uint32_t                perturbation;
    uint32_t                quantum;
    codel_params_t          codel_params;
    codel_stats_t           codel_stats;
    uint32_t                limit;
    //uint32_t              memory_usage;
    uint32_t                drop_overlimit;
    uint32_t                new_flow_count;
    struct list_head        new_flows;
    struct list_head        old_flows;
}fqcodel_schedule_data_t;


fqcodel_schedule_data_t *fqcodel_init();

uint32_t fqcodel_hash(fqcodel_schedule_data_t *fqCodel, const void *iphdr, size_t len);

int fqcodel_enqueue(fqcodel_schedule_data_t *fqCodel, reserve_frames_block_t *block);

reserve_frames_block_t *fqcodel_dequeue(fqcodel_schedule_data_t *fqcodel);

reserve_frames_block_t *fqcodel_peek(fqcodel_schedule_data_t *fqcodel);

//function to print basic infromation for debugging purposes
void fq_print_stats(fqcodel_schedule_data_t *fq_codel);

static inline void fqcodel_destroy(fqcodel_schedule_data_t *fqcodel)
{
    free(fqcodel->backlogs);
    free(fqcodel->flows);
    free(fqcodel);
}

#endif /* FQCODEL_H */