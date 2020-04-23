#include "rttbpf.h"

protoop_arg_t parse_timestamp_frame(picoquic_cnx_t *cnx)
{
    uint8_t* bytes = (uint8_t *) get_cnx(cnx, AK_CNX_INPUT, 0);
    const uint8_t* bytes_max = (uint8_t *) get_cnx(cnx, AK_CNX_INPUT, 1);

    rtt_timestamp_frame_t *frame = my_malloc(cnx, sizeof(rtt_timestamp_frame_t));
    if (!frame) return PICOQUIC_ERROR_MEMORY;

    if ((bytes = picoquic_frames_varint_decode(bytes + 1, bytes_max, &frame->stamp)) == NULL){
	PROTOOP_PRINTF(cnx, "TIMESTAMP FRAME DECODE ERROR\n");
	my_free(cnx, frame);
    	frame = NULL;
    }

    //PROTOOP_PRINTF(cnx, "Parse Timestamp packet with SRTT = %d\n", frame->srtt);
    set_cnx(cnx, AK_CNX_OUTPUT, 0, (protoop_arg_t) frame);
    set_cnx(cnx, AK_CNX_OUTPUT, 1, false);	//no ack
    set_cnx(cnx, AK_CNX_OUTPUT, 2, false);	//no retransmission
    return (protoop_arg_t) bytes;
}
