#include "memory.h"
#include "memcpy.h"
#include "../../helpers.h"

#define FT_RTTIMESTAMP 0x43
#define RTTIMESTAMP_OPAQUE_ID 0x01
#define MAX_PATHS 4

typedef struct rtt_timestamp_frame {
    uint64_t stamp;
} rtt_timestamp_frame_t;

typedef struct st_rttimestamp_memory_t {
    uint64_t this_stamp;	//only used in RTT trasmission not for 1wd (add_timestamp_frame.c)
    uint64_t max1wdiff;
    uint64_t path_1wdiff[MAX_PATHS];
    uint8_t path_id[MAX_PATHS];
    uint8_t num_paths;
} rttimestamp_memory_t;

static __attribute__((always_inline)) rttimestamp_memory_t *initialize_memory(picoquic_cnx_t *cnx)  // TODO: We need to free it as well
{
    rttimestamp_memory_t *metrics = (rttimestamp_memory_t *) my_malloc(cnx, sizeof(rttimestamp_memory_t));
    if (!metrics) return NULL;
    my_memset(metrics, 0, sizeof(rttimestamp_memory_t));
    return metrics;
}

static __attribute__((always_inline)) rttimestamp_memory_t *get_memory(picoquic_cnx_t *cnx)
{
    int allocated = 0;
    rttimestamp_memory_t **bpfd_ptr = (rttimestamp_memory_t **) get_opaque_data(cnx, RTTIMESTAMP_OPAQUE_ID, sizeof(rttimestamp_memory_t *), &allocated);
    if (!bpfd_ptr) return NULL;
    if (allocated) {
        *bpfd_ptr = initialize_memory(cnx);
        (*bpfd_ptr)->num_paths = 0;
        (*bpfd_ptr)->max1wdiff = 125000;			//half of default rtt value
        my_memset((*bpfd_ptr)->path_id, 0, sizeof((*bpfd_ptr)->path_id));
	my_memset((*bpfd_ptr)->path_1wdiff, 0, sizeof((*bpfd_ptr)->path_1wdiff));
    }
    return *bpfd_ptr;
}

static __attribute__((always_inline)) uint8_t process_path_ids(picoquic_cnx_t *cnx, picoquic_connection_id_t *cnx_id) {
    rttimestamp_memory_t *m = get_memory(cnx);
    uint8_t i = 0;
    while(i < MAX_PATHS){
        if(m->path_id[i] == (uint8_t) cnx_id->id){
            return i;
        }
        if(m->path_id[i] == 0){
            m->path_id[i] = (uint8_t) cnx_id->id;
            m->num_paths = i + 1;
            return i;
        }
        i++;
    }
    return 0;
}

static uint64_t maxValue(uint64_t myArray[], int size) {
    uint64_t maxValue = myArray[0];
    for (int i = 1; i < size; ++i) {
        if ( myArray[i] > maxValue ) maxValue = myArray[i];
    }
    return maxValue;
}

static uint64_t minValue(uint64_t myArray[], int size) {
    uint64_t minValue = myArray[0];
    for (int i = 1; i < size; ++i) {
        if ( myArray[i] < minValue ) minValue = myArray[i];
    }
    return minValue;
}


static __attribute__((always_inline)) uint64_t get_max_1wdifference(rttimestamp_memory_t *tm){
    uint64_t min1wdiff = minValue(tm->path_1wdiff, tm->num_paths);
    uint64_t max1wdiff = maxValue(tm->path_1wdiff, tm->num_paths);
    if(min1wdiff == 0 || max1wdiff == 0) return 0;
    else return max1wdiff - min1wdiff;

/*        if(tm->path_1wdiff[id_1] == 0 || tm->path_1wdiff[id_2] == 0) return 0;
        if(tm->path_1wdiff[id_1] > tm->path_1wdiff[id_2]) return tm->path_1wdiff[id_1] - tm->path_1wdiff[id_2];
        else return tm->path_1wdiff[id_2] - tm->path_1wdiff[id_1];
*/
}

