#include "fqCoDel.h"
#include "plugin.h"
#include "memory.h"
#include "picoquic_internal.h"
//value common bdp --> 50 mbit/s * 26 ms
#define STATIC_BDP 162500
//#define QUEUE_SIZE 375000
#define QUEUE_SIZE 10000000

#define DBG false

#ifndef NUM_FLOWS
#define SHIFT 6
#define NUM_FLOWS 2 << SHIFT
#endif

void printQueueStats(picoquic_cnx_t *cnx, fqcodel_schedule_data_t *f)
{   
    if(DBG)
    {
        uint64_t t = picoquic_current_time();
        printf("\n Active Flows: %d\n",f->new_flow_count);
        printf("TIME: %6ld\n",t);
        printf("\nBUFFER: %d %lu\n", f->limit, t);
        printf("CoDel %ld\n", f->codel_params.interval);
        for(int i = 0; i < NUM_FLOWS; i++)
        {   
            //  printf("\tQ: %2u BACKLOG %6d\n",
            //         i, f->backlogs[i]);
            printf("\tQ: %2u BACKLOG %6d SIZE %6lu CREDITS %6d %lu\n",
                    i, f->backlogs[i], queue_size(f->flows[i].codel_queue), f->flows[i].credits, t);
        }
    }
}




//returns the index to the queue to which the dataflow belongs to
inline uint32_t fqcodel_hash(fqcodel_schedule_data_t *fqcodel, const void *hashKey, size_t len)
{
    return hashlittle(hashKey,  sizeof(uint32_t), 1) & hashmask((int)SHIFT);
}

//check if there are any flows alive
struct list_head * peek_flow_queues(picoquic_cnx_t *cnx, fqcodel_schedule_data_t *fqcodel)
{
    struct list_head *head;
    head = &fqcodel->new_flows;
    if(list_empty(head))
    {
        head = &fqcodel->old_flows;
        if(list_empty(head)) return NULL;
    }
    return head;
}

void fqcodel_drop(picoquic_cnx_t *cnx, fqcodel_schedule_data_t *fqcodel)
{   
    fqcodel_flow_t *flow;
    codel_frame_t *frame;
    uint32_t max_backlog = 0;
    uint32_t idx = 0;
    for(int i = 0; i < NUM_FLOWS; i++)
    {
		if (fqcodel->backlogs[i] > max_backlog) 
        {
			max_backlog = fqcodel->backlogs[i];
			idx = i;
		}
    }
    flow = &fqcodel->flows[idx];   
    
    frame = queue_dequeue(flow->codel_queue);
    uint32_t allowed_buffered_bytes = QUEUE_SIZE/fqcodel->new_flow_count;
    uint32_t bytes_dropped = frame->block->total_bytes;
    uint32_t frames_dropped = 1;

    max_backlog -= bytes_dropped;
	while (max_backlog > allowed_buffered_bytes)
    {
        frame = queue_dequeue(flow->codel_queue);
        if(!frame) continue;
        free(frame->block);
        free(frame);
        bytes_dropped += frame->block->total_bytes;
        max_backlog -= bytes_dropped;
        frames_dropped++;
    }


    fqcodel->drop_overlimit += frames_dropped;
    fqcodel->backlogs[idx] -= bytes_dropped;
    fqcodel->limit -= bytes_dropped;
    printQueueStats(cnx, fqcodel);
    if(DBG)printf("DROPPED_MEMORY: %6d %lu\n", fqcodel->drop_overlimit, picoquic_current_time());

    protoop_id_t pid;
    pid.id = "update_frames_dropped";
    pid.hash = hash_value_str(pid.id);
    protoop_prepare_and_run_noparam(cnx, &pid, NULL, bytes_dropped, frames_dropped);
    return;
}

//Enqueing into FQ_CoDel
int fqcodel_enqueue(picoquic_cnx_t *cnx, fqcodel_schedule_data_t *fqcodel, reserve_frames_block_t  *block)
{   
    //Classify data into the appropriate queue
    uint32_t idx = fqcodel_hash(fqcodel,&block->frames->fq_key, IP_HEADER_PARAMS);
    fqcodel_flow_t *flow = &fqcodel->flows[idx];
    codel_frame_t *codel_frame = malloc(sizeof(codel_frame_t));

    if(codel_frame == NULL)  {return 1;}
    //enqueue the codel frame into FQ
    codel_frame->block = block;
    codel_frame->enqueue_time = picoquic_current_time();
    //update backlogs

    if(queue_enqueue(flow->codel_queue, codel_frame) == 1) {return 1;}
    fqcodel->backlogs[idx] += block->total_bytes;
    fqcodel->limit += block->total_bytes;
    //Flow Queue management
    //if the flow is empty append it to list of new flows and update the number of new flows
    if(list_empty(&flow->flow_chain)){
        list_add_tail(&flow->flow_chain, &fqcodel->new_flows);
        fqcodel->new_flow_count++;
        //update flow vars
        //weflow->dropped = 0;
        flow->credits = fqcodel->quantum;        
    }
    if(fqcodel->backlogs[idx] >= QUEUE_SIZE/fqcodel->new_flow_count)fqcodel_drop(cnx, fqcodel);
    printQueueStats(cnx, fqcodel);
    return 0;                    
}

//Peek if there are any frames that can be scheduled
reserve_frames_block_t *fqcodel_peek(picoquic_cnx_t *cnx, fqcodel_schedule_data_t *fqcodel)
{   
    codel_frame_t *frame = NULL;
    fqcodel_flow_t *flow = NULL;
    struct list_head *head = NULL;
    head = peek_flow_queues(cnx, fqcodel);
    if(!head){
        //printf("HEAD NULL\n"); 
        return NULL;
    } 
    //check if there are frames to be scheduled in the choosen flow 
    flow = list_first_entry(head, fqcodel_flow_t, flow_chain);
    frame = queue_peek(flow->codel_queue);
    if(!frame)
    {
        list_move_tail(&flow->flow_chain, &fqcodel->old_flows);
        return NULL;
    }    
    return frame->block;   
}

codel_frame_t * dequeue_function(fqcodel_schedule_data_t *fqcodel, fqcodel_flow_t *flow)
{
    codel_frame_t *frame = queue_dequeue(flow->codel_queue);    
    if(!frame) return NULL;

    fqcodel->backlogs[flow - fqcodel->flows] -= frame->block->total_bytes;
    fqcodel->limit -= frame->block->total_bytes;
    return frame;
}

void drop_function(picoquic_cnx_t *cnx, fqcodel_schedule_data_t *fqcodel, codel_frame_t *frame)
{ 
    fqcodel->drop_overlimit++;
    if(DBG)printf("DROPPED_CODEL: %6d %lu\n", 0, picoquic_current_time());

    //relay dropped framed to datagram plugin
    protoop_id_t pid;
    pid.id = "update_frames_dropped";
    pid.hash = hash_value_str(pid.id);
    protoop_prepare_and_run_noparam(cnx, &pid, NULL, frame->block->total_bytes, 1);
    free(frame->block);
    free(frame);
    return;
}



static bool codel_should_drop(fqcodel_schedule_data_t *fqcodel, fqcodel_flow_t *flow, codel_frame_t *frame, uint64_t now)
{
	bool ok_to_drop;
	flow->codel_vars.lDelay = now - frame->enqueue_time;
	if (flow->codel_vars.lDelay < fqcodel->codel_params.target) {
		/* went below - stay below for at least interval */
		flow->codel_vars.first_above_time = 0;
		return false;
	}
	ok_to_drop = false;
	if (flow->codel_vars.first_above_time == 0) {
		/* just went above from below. If we stay above
		 * for at least interval we'll say it's ok to drop
		 */
		flow->codel_vars.first_above_time = now + fqcodel->codel_params.interval;
	} else if (now > flow->codel_vars.first_above_time) {
		ok_to_drop = true;
	}
	return ok_to_drop;
}

codel_frame_t *codel_dequeue(picoquic_cnx_t *cnx, fqcodel_schedule_data_t *fqcodel, fqcodel_flow_t *flow)
{
    codel_frame_t *frame = dequeue_function(fqcodel,flow);    
    if(!frame) return NULL;

	uint64_t now = picoquic_current_time();
	bool drop = codel_should_drop(fqcodel, flow, frame, now);
    if(flow->codel_vars.dropping)
    {
        if(!drop)
        {
            flow->codel_vars.dropping = false;
        }
        else if(now > flow->codel_vars.drop_next)
        {
            while (now > flow->codel_vars.drop_next && flow->codel_vars.dropping)
            {
                drop_function(cnx, fqcodel, frame);

                flow->codel_vars.count++;
                frame = dequeue_function(fqcodel, flow);
                if(!frame) return NULL;
                drop = codel_should_drop(fqcodel, flow, frame, now);
                if(DBG)printf("DROPPING STATE: %lu\n", flow->codel_vars.lDelay);
                if(!drop)
                {
                    flow->codel_vars.dropping = false;
                    if(DBG)printf("EXIT DROPPING STATE\n");
                }
                else
                {
                    flow->codel_vars.drop_next = flow->codel_vars.drop_next + fqcodel->codel_params.interval / flow->codel_vars.count;
                    //(fqcodel->codel_params.interval * flow->codel_vars.rec_inv_sqrt << ((8 * sizeof(flow->codel_vars.rec_inv_sqrt)) >> 32 ));
                    if(DBG)printf("DROPNEXT %lu\n", flow->codel_vars.drop_next);
                }                
            }           
        } 
    }
    else if (drop) {
        //drop packet
        drop_function(cnx, fqcodel, frame);
        if(DBG)printf("ENTER DROPPING STATE\n");
        flow->codel_vars.count++;
        frame = dequeue_function(fqcodel, flow);
        if(!frame) return NULL;

        drop = codel_should_drop(fqcodel, flow, frame, now);
        flow->codel_vars.dropping = true;
        if(flow->codel_vars.lDelay < fqcodel->codel_params.interval)
        {
            flow->codel_vars.count = flow->codel_vars.count > 2 ? flow->codel_vars.count - 2 : 1;
        }
        else
        {
            flow->codel_vars.count = 1;
        }
        flow->codel_vars.drop_next = now + fqcodel->codel_params.interval / flow->codel_vars.count; 
        //(fqcodel->codel_params.interval * flow->codel_vars.rec_inv_sqrt << ((8 * sizeof(flow->codel_vars.rec_inv_sqrt)) >> 32 ));	
        if(DBG)printf("DROPNEXT %lu\n", flow->codel_vars.drop_next);
     }
    if(DBG)printf("SJOURN_TIME %lu %lu %ld\n",flow->codel_vars.lDelay, picoquic_current_time(), flow - fqcodel->flows);
	return frame;
}


//Dequeue frames from FQ_CoDel
reserve_frames_block_t *fqcodel_dequeue(picoquic_cnx_t *cnx, fqcodel_schedule_data_t *fqcodel)
{
    //printQueueStats(fqcodel);
    struct list_head *head;
    codel_frame_t *frame = NULL;
    fqcodel_flow_t *flow;  

    //while no frame is dequeued
    while (!frame)
    {
        //Try peek for active queues in New and Old flow queue 
        head = peek_flow_queues(cnx, fqcodel);
        if(!head) return NULL;
        //Get flow from the choosen queue
        flow = list_first_entry(head, fqcodel_flow_t, flow_chain);
        //Check if the flow has enough credits to send, else place last in old_flows
        if (flow->credits <= 0) 
        {
            flow->credits += fqcodel->quantum;
            list_move_tail(&flow->flow_chain, &fqcodel->old_flows);
            continue;
        }
        //if the flow has data to send move it last in new flows to keep fair
        frame = codel_dequeue(cnx, fqcodel, flow);
        
       //if flow is new and there exists frames in the old flows --> move current flow to old flows to prevent starvation
        if(!frame)
        {
            //if flow is new and there exists frames in the old flows --> move current flow to old flows to prevent starvation
            if ((head == &fqcodel->new_flows) && !list_empty(&fqcodel->old_flows))
                list_move_tail(&flow->flow_chain, &fqcodel->old_flows);
            //if no frame no frames to be scheduled --> delete the flow
            else
            {
                list_del_init(&flow->flow_chain);
                --fqcodel->new_flow_count;
            }
        }        
    }
    //Update the choosen flows credits
    flow->credits -= frame->block->total_bytes;
    //fqcodel->limit -= frame->block->total_bytes;

    //fqcodel->backlogs[flow - fqcodel->flows] -= frame->block->total_bytes;
    printQueueStats(cnx, fqcodel);
    //printf("TIME: %lu", frame->enqueue_time);
    return frame->block;
}



fqcodel_schedule_data_t *fqcodel_init()
{
    fqcodel_schedule_data_t *fqcodel = malloc(sizeof(fqcodel_schedule_data_t));
    if(fqcodel == NULL)
    {
        return NULL;
    }
    //set limit of number of flows
    fqcodel->flows_count = NUM_FLOWS;
    //set the quantum bytes to the MTU = 1400 bytes
    fqcodel->quantum = MAX_MTU;
    fqcodel->perturbation = 1; //TODO: use a random number instead
    fqcodel->new_flow_count=0;
    fqcodel->limit = 0;
    fqcodel->drop_overlimit = 0;
    //init list for new and old flows
    INIT_LIST_HEAD(&fqcodel->new_flows);
    INIT_LIST_HEAD(&fqcodel->old_flows);

    //initiate the CoDel params and stats according to the linux implementation
    init_codel_params(&fqcodel->codel_params);
    
    //allocate memory for flowsCount number of flows
    fqcodel->flows = malloc( fqcodel->flows_count * sizeof(fqcodel_flow_t));
    if(fqcodel == NULL)
    {
        //printf("Memory allocation of flows FAILED");
        return NULL;
    }
    fqcodel->backlogs = malloc(fqcodel->flows_count * sizeof(uint32_t));
    if(fqcodel->backlogs == NULL)
    {
        //printf("Memory alloacation for backlogs FAILED");
    }
    for(int i = 0; i < fqcodel->flows_count; i++)
    {
        fqcodel_flow_t *f = fqcodel->flows + i;
        f->codel_queue = queue_init();
        fqcodel->backlogs[i] = 0;
        fqcodel->flows[i].credits = 0;
        INIT_LIST_HEAD(&f->flow_chain);        
        init_codel_vars(&f->codel_vars);
    }
    return fqcodel;
}