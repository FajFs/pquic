#include "fqCoDel.h"


//TODO: move to separate file, should not be here
int parse_ipheader(ipheader_t *iphdr, uint8_t *ip_packet)
{
    //check if ip or icmp

    //check if ipv4 or 6


    //simple parse of ipv4 header
    memcpy(&iphdr->diffserv, ip_packet               + 1, sizeof(uint8_t));
    memcpy(&iphdr->protocol, ip_packet               + 9, sizeof(uint8_t));
    memcpy(&iphdr->source_ip, ip_packet              + 12, sizeof(uint32_t));
    memcpy(&iphdr->destination_ip, ip_packet         + 16, sizeof(uint32_t));
    memcpy(&iphdr->source_port, ip_packet            + 20, sizeof(uint16_t));
    memcpy(&iphdr->destination_port, ip_packet       + 22, sizeof(uint16_t));
    //hacky solution but it works
    iphdr->hashkey[0] =  (uint32_t) iphdr->protocol;
    iphdr->hashkey[1] =             iphdr->source_ip;
    iphdr->hashkey[2] =             iphdr->destination_ip;
    iphdr->hashkey[3] =  (uint32_t) iphdr->source_port;
    iphdr->hashkey[4] =  (uint32_t) iphdr->destination_port;

    return 1;
}



//returns the index to the queue to which the dataflow belongs to
// out: 0 - 2^10 -1 
inline uint32_t fqcodel_hash(fqcodel_schedule_data_t *fqcodel, const void *hashKey, size_t len)
{
    //allows for mapping of 1024 concurrent flows, currently hashing on only is congestion controlled to test
    uint32_t t = hashlittle(hashKey,  sizeof(uint8_t), 1) & hashmask(2);
    //printf("the hash value = %u\n",t);
    return t;
}

void fq_print_stats(fqcodel_schedule_data_t *fqcodel)
{
    codel_frame_t *frame = (codel_frame_t *) queue_peek(fqcodel->flows[722].codel_queue);
    printf("Enqueue Time: %lu\n", frame->enqueue_time);
    printf("should equal 0x2e == %lx\n",frame->block->frames[0].frame_type);
}





//Enqueing
int fqcodel_enqueue(fqcodel_schedule_data_t *fqcodel, reserve_frames_block_t  *block)
{   
    int ret;
    //FIX ME!!!
    uint32_t i = 1;
    uint32_t *test = &i;
    //start by classifying the data into a queue, the plugin specific data is the 5-touple
    uint32_t idx = fqcodel_hash(fqcodel, test, IP_HEADER_PARAMS); //hacky but works since the datagram plugin only schedules 1 frame at a time

    fqcodel_flow_t *flow = &fqcodel->flows[idx];

    codel_frame_t *cf = malloc(sizeof(codel_frame_t));
    if(cf == NULL)
    {
        printf("Memory allocation of codelFrame FAILED");
        return 0;
    }

    cf->enqueue_time =  picoquic_current_time();
    cf->block = block;
    //update backlogs
    fqcodel->backlogs[idx] += block->total_bytes;

    ret = queue_enqueue(flow->codel_queue, cf);
    if(ret != 0)
    {
        printf("Enqueing of data failed\n");
        return ret;
    }
    //if the flow is empty append it to list of new flows
    if(list_empty(&flow->flow_chain)){
        list_add_tail(&flow->flow_chain, &fqcodel->new_flows);
        //update number of new flows
        fqcodel->new_flow_count++;
        //update flow vars
        flow->dropped = 0;
        flow->credits = fqcodel->quantum;
    }
    // //append data to flow
    // printf("\tHASH/QUEUE: %d\n",idx);
    // printf("\tBACKLOG: %d\n",fqcodel->backlogs[idx]);
    return ret;
}

//function very similar to fqcodel_dequeue, i am lazy, FIXME!!!
reserve_frames_block_t *fqcodel_peek(fqcodel_schedule_data_t *fqcodel)
{
    // printf("PEEKING FOR FRAME IN FQCODEL\n");
    codel_frame_t *frame;
    fqcodel_flow_t *flow;
    struct list_head *head;

    head = &fqcodel->new_flows;
    if(list_empty(head))
    {
        head = &fqcodel->old_flows;
        if(list_empty(head))
        {
            // printf("ALL QUEUES EMPTY\n");
            return NULL;
        }
    }

    flow = list_first_entry(head, fqcodel_flow_t, flow_chain);
    if((frame = queue_peek(flow->codel_queue)) == NULL){
        printf("The current queue is empty!\n");
        return NULL;
    }
    // printf("FQCODEL HAS AT LEAST ONE DEQUABLE FRAME (PEEK)\n\t BYTES TO SEND: %lu\n", frame->block->total_bytes);

    return frame->block;   
}

reserve_frames_block_t *fqcodel_dequeue(fqcodel_schedule_data_t *fqcodel){
    codel_frame_t *frame;
    fqcodel_flow_t *flow;
    struct list_head *head;

    head = &fqcodel->new_flows;
    if(list_empty(head))
    {
        head = &fqcodel->old_flows;
        if(list_empty(head))
        {
            // printf("ALL QUEUES IN FQCODEL CURRENTLY EMPTY\n");
            return NULL;
        }
    }

    flow = list_first_entry(head, fqcodel_flow_t, flow_chain);

    if(queue_peek(flow->codel_queue) == NULL){
        printf("The current queue is empty\n!");
        return NULL;
    }

    frame = queue_dequeue(flow->codel_queue);
    //update backlog and other data
    // printf("FQCODEL HAS DEQUEUED FRAME (DEQEUE)\n");
    return frame->block;
}



fqcodel_schedule_data_t *fqcodel_init()
{
    fqcodel_schedule_data_t *fqcodel = malloc(sizeof(fqcodel_schedule_data_t));
    if(fqcodel == NULL)
    {
        printf("Memory allocation of fq CoDel Schedule Data FAILED\n");
        return NULL;
    }
    //set limit of number of flows
    fqcodel->flows_count = NUM_FLOWS;
    //set the quantum bytes to the MTU = 1400 bytes
    //TODO: cope with varying MTUs
    fqcodel->quantum = 1400;
    fqcodel->perturbation = 1; //TODO: use a random number instead

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
        printf("Memory allocation of flows FAILED");
        return NULL;
    }
    fqcodel->backlogs = malloc(fqcodel->flows_count * sizeof(uint32_t));
    if(fqcodel->backlogs == NULL)
    {
        printf("Memory alloacation for backlogs FAILED");
    }
    for(int i = 0; i < fqcodel->flows_count; i++)
    {
        fqcodel_flow_t *f = fqcodel->flows + i;
        f->codel_queue = queue_init();
        INIT_LIST_HEAD(&f->flow_chain);       
        //init_codel_vars(&f->codel_vars);
    }
    return fqcodel;
}