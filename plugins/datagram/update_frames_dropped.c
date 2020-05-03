#include "../helpers.h"
#include "bpf.h"


protoop_arg_t update_frames_dropped(picoquic_cnx_t* cnx)
{
    int bytes_dropped = (int) get_cnx(cnx, AK_CNX_INPUT, 0);
    int frames_dropped = (int) get_cnx(cnx, AK_CNX_INPUT, 1);
    
    datagram_memory_t *m = get_datagram_memory(cnx);
    m->send_buffer -= bytes_dropped - (3 * frames_dropped);

    PROTOOP_PRINTF(cnx, "DATAGRAM BYTES TO DROP: %d\n", bytes_dropped);

}