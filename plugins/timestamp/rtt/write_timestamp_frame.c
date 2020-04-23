#include "../bpf.h"
#include "rttbpf.h"

protoop_arg_t write_timestamp_frame(picoquic_cnx_t *cnx)
{
    uint8_t* bytes = (uint8_t *) get_cnx(cnx, AK_CNX_INPUT, 0);
    const uint8_t* bytes_max = (const uint8_t *) get_cnx(cnx, AK_CNX_INPUT, 1);
    rtt_timestamp_frame_t* frame = (rtt_timestamp_frame_t*) get_cnx(cnx, AK_CNX_INPUT, 2);
    uint64_t now = picoquic_current_time();


    int id_size = 1;

    int ret = 0;
    size_t consumed = 1;

    datagram_memory_t *m = get_datagram_memory(cnx);
    //if (m->this_srtt == 0 || m->this_srtt == 250000) goto exit;
    //frame->stamp = m->this_srtt;
    frame->stamp = now;


    if ((bytes_max - bytes) < id_size + varint_len(frame->stamp)) {
        ret = PICOQUIC_ERROR_FRAME_BUFFER_TOO_SMALL;
        goto exit;
    }

    my_memset(bytes, FT_RTTIMESTAMP, 1);
    //consumed += picoquic_varint_encode(bytes, id_size, FT_RTTIMESTAMP);
    consumed += picoquic_varint_encode(bytes + id_size, bytes_max - (bytes + id_size), frame->stamp);

exit:
    set_cnx(cnx, AK_CNX_OUTPUT, 0, (protoop_arg_t) consumed);
    set_cnx(cnx, AK_CNX_OUTPUT, 1, (protoop_arg_t) 0); //no retransmit
    return (protoop_arg_t) ret;
}
