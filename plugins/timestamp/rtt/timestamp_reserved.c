#include "rttbpf.h"

protoop_arg_t reserve_timestamp_frame(picoquic_cnx_t *cnx)
{
    int id_size = 1;

    reserve_frame_slot_t *slot = (reserve_frame_slot_t *) my_malloc(cnx, sizeof(reserve_frame_slot_t));
    if (!slot) return 1;
    my_memset(slot, 0, sizeof(reserve_frame_slot_t));
    slot->frame_type = FT_RTTIMESTAMP;
    slot->nb_bytes = id_size + sizeof(rtt_timestamp_frame_t);
  //slot->is_congestion_controlled = false;
  //slot->low_priority = false;

    rtt_timestamp_frame_t *f = (rtt_timestamp_frame_t *) my_malloc(cnx, sizeof(rtt_timestamp_frame_t));
    if (!f) return 1;
    slot->frame_ctx = f;
    size_t reserved_size = reserve_frames(cnx, 1, slot);

    if (reserved_size < slot->nb_bytes) {
        my_free(cnx, f);
        my_free(cnx, slot);
    }
    return 0;
}


