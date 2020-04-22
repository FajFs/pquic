#include "fqCoDel.h"

//value common bdp --> 50 mbit/s * 26 ms
#define STATIC_BDP 162500
//#define QUEUE_SIZE 375000
#define QUEUE_SIZE 125000*2

void printQueueStats(fqcodel_schedule_data_t *f)
{
    uint64_t t = picoquic_current_time();
    printf("\n Active Flows: %d\n",f->new_flow_count);
    printf("TIME: %6ld\n",t);
    printf("\nBUFFER: %d %lu\n", f->limit, t);

    printf("DROPPED: %6d %lu\n", f->drop_overlimit, t);
    for(int i = 0; i < NUM_FLOWS; i++)
    {   
        //  printf("\tQ: %2u BACKLOG %6d\n",
        //         i, f->backlogs[i]);
        printf("\tQ: %2u BACKLOG %6d SIZE %6lu CREDITS %6d %lu\n",
                i, f->backlogs[i], queue_size(f->flows[i].codel_queue), f->flows[i].credits, t);
    }
}




//returns the index to the queue to which the dataflow belongs to
inline uint32_t fqcodel_hash(fqcodel_schedule_data_t *fqcodel, const void *hashKey, size_t len)
{
    return hashlittle(hashKey,  sizeof(uint32_t), 1) & hashmask(2);
}

//check if there are any flows alive
struct list_head * peek_flow_queues(fqcodel_schedule_data_t *fqcodel)
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

int fqcodel_drop(fqcodel_schedule_data_t *fqcodel)
{   
    fqcodel_flow_t *flow;
    codel_frame_t *frame;
    codel_frame_t *tmp_frame;
    uint32_t max_backlog = 0;
    uint32_t idx = 0;
    uint32_t limit = 0;
    for(int i = 0; i < NUM_FLOWS; i++)
    {
		if (fqcodel->backlogs[i] > max_backlog) 
        {
			max_backlog = fqcodel->backlogs[i];
			idx = i;
		}
    }
    flow = &fqcodel->flows[idx];   
    limit = QUEUE_SIZE/fqcodel->new_flow_count;
    
    frame = queue_dequeue(flow->codel_queue);
    max_backlog -= frame->block->total_bytes;
    fqcodel->drop_overlimit++;
	while (max_backlog > limit)
    {
        tmp_frame = queue_dequeue(flow->codel_queue);
        frame->block->total_bytes += tmp_frame->block->total_bytes;
        max_backlog -= tmp_frame->block->total_bytes;
        fqcodel->drop_overlimit++;

    }
     
    fqcodel->backlogs[idx] -= frame->block->total_bytes;
    fqcodel->limit -= frame->block->total_bytes;
    printQueueStats(fqcodel);
    return frame->block->total_bytes;
}

//Enqueing into FQ_CoDel
int fqcodel_enqueue(fqcodel_schedule_data_t *fqcodel, reserve_frames_block_t  *block)
{   
    //Classify data into the appropriate queue
    uint32_t idx = fqcodel_hash(fqcodel,&block->frames->fq_key, IP_HEADER_PARAMS);
    fqcodel_flow_t *flow = &fqcodel->flows[idx];
    codel_frame_t *codel_frame = malloc(sizeof(codel_frame_t));

    if(codel_frame == NULL)  {return 1;}
    //enqueue the codel frame into FQ
    codel_frame->block = block;
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
    //check for fat flows
    //if(fqcodel->limit >= QUEUE_SIZE) block->frames->nb_frames_to_drop = fqcodel_drop(fqcodel);

    if(fqcodel->backlogs[idx] >= QUEUE_SIZE/fqcodel->new_flow_count) block->frames->nb_frames_to_drop = fqcodel_drop(fqcodel);
    printQueueStats(fqcodel);
    return 0;                    
}

//Peek if there are any frames that can be scheduled
reserve_frames_block_t *fqcodel_peek(fqcodel_schedule_data_t *fqcodel)
{   
    codel_frame_t *frame = NULL;
    fqcodel_flow_t *flow = NULL;
    struct list_head *head = NULL;
    head = peek_flow_queues(fqcodel);
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


//Dequeue frames from FQ_CoDel
reserve_frames_block_t *fqcodel_dequeue(fqcodel_schedule_data_t *fqcodel)
{
    //printQueueStats(fqcodel);
    struct list_head *head;
    codel_frame_t *frame = NULL;
    fqcodel_flow_t *flow;  

    //while no frame is dequeued
    while (!frame)
    {
        //Try peek for active queues in New and Old flow queue 
        head = peek_flow_queues(fqcodel);
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
        frame = queue_dequeue(flow->codel_queue);
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
    fqcodel->limit -= frame->block->total_bytes;

    fqcodel->backlogs[flow - fqcodel->flows] -= frame->block->total_bytes;
    printQueueStats(fqcodel);
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
    //init_codel_params(&fqcodel->codel_params);
    //init_codel_stats(&fqcodel->codel_stats);
    //->codel_params.ecn = true;
    //fqcodel->codel_params.mtu = MAX_MTU; //TODO: cope with varying MTUs

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
        INIT_LIST_HEAD(&f->flow_chain);        //init_codel_vars(&f->codel_vars);
    }
    return fqcodel;
}