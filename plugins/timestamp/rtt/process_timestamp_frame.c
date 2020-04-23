#include "rttbpf.h"
#include "../bpf.h"

protoop_arg_t process_timestamp_frame(picoquic_cnx_t *cnx)
{
    rtt_timestamp_frame_t *frame = (rtt_timestamp_frame_t *) get_cnx(cnx, AK_CNX_INPUT, 0);
    picoquic_path_t *path_x = (picoquic_path_t*) get_cnx(cnx, AK_CNX_INPUT, 3);

    //PROTOOP_PRINTF(cnx, "Received TIMESTAMP - %d\n",frame->stamp);

    uint64_t ts_now = picoquic_current_time(); // could consider ack delay and phase shift
    rttimestamp_memory_t *tm = get_memory(cnx);

    picoquic_connection_id_t *local_cnxid = (picoquic_connection_id_t *) get_path(path_x, AK_PATH_LOCAL_CID, 0);
    uint8_t path_index = process_path_ids(cnx, local_cnxid);
    PROTOOP_PRINTF(cnx, "Path %d of %d\n", path_index+1, tm->num_paths);


	//*filter inspired by rtt computation to gradually smoothen our 1wd *//
    tm->path_1wdiff[path_index] = frame->stamp - ts_now;
    int64_t delta_ts = get_max_1wdifference(tm) - tm->max1wdiff;
    tm->max1wdiff = (uint64_t) ((int64_t) tm->max1wdiff + delta_ts / 8);

    PROTOOP_PRINTF(cnx, "new max 1WD DIFFERENCE %d, delta %d index %d\n", tm->max1wdiff, delta_ts, path_index);

    return (protoop_arg_t) 0;
}
