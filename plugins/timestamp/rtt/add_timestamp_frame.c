#include "../bpf.h"
#include "rttbpf.h"

protoop_arg_t schedule_frames_on_path(picoquic_cnx_t *cnx)
{
    picoquic_path_t* path_x = (picoquic_path_t*) get_cnx(cnx, AK_CNX_OUTPUT, 0);
    rttimestamp_memory_t *tm = get_memory(cnx);

    picoquic_packet_t *packet = (picoquic_packet_t *) get_cnx(cnx, AK_CNX_INPUT, 0);
    picoquic_packet_type_enum ptype = get_pkt(packet, AK_PKT_TYPE);
    uint32_t len = get_pkt(packet, AK_PKT_LENGTH);

    uint64_t prev = tm->this_stamp;
    if( prev != tm->this_stamp) PROTOOP_PRINTF(cnx, "updating srtt=%d to TIMESTAMP memory\n", get_path(path_x, AK_PATH_SMOOTHED_RTT, 0));
    tm->this_stamp = get_path(path_x, AK_PATH_SMOOTHED_RTT, 0);
    return 0;
}

