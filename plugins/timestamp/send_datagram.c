#include "../helpers.h"
#include "bpf.h"


protoop_arg_t send_datagram_frame(picoquic_cnx_t* cnx)
{
    char *payload = (char *) get_cnx(cnx, AK_CNX_INPUT, 0);
    int len = (int) get_cnx(cnx, AK_CNX_INPUT, 1);
    uint64_t datagram_id = 0;
  //uint64_t datagram_path_id = 999;
    datagram_memory_t *m = get_datagram_memory(cnx);

    uint32_t max_path_mtu = get_max_datagram_size(cnx);
    if (len > max_path_mtu) {
        PROTOOP_PRINTF(cnx, "Unable to send %d-byte long message, max known payload transmission unit is %d bytes\n", len, max_path_mtu);
        return 1;
    }

    reserve_frame_slot_t *slot = (reserve_frame_slot_t *) my_malloc_on_sending_buffer(m, cnx, sizeof(reserve_frame_slot_t));
    if (slot == NULL) {
        //PROTOOP_PRINTF(cnx, "Unable to allocate frame slot!\n");
        return 1;
    }
    my_memset(slot, 0, sizeof(reserve_frame_slot_t));
#ifdef DATAGRAM_WITH_ID
    datagram_id = ++get_datagram_memory(cnx)->next_datagram_id;
    slot->frame_type = FT_DATAGRAM | FT_DATAGRAM_ID | FT_DATAGRAM_LEN;
    slot->nb_bytes = 1 + varint_len(datagram_id) + varint_len(len) + len;  // Unfortunately we are always forced to account for the length field
#else
    slot->frame_type = FT_DATAGRAM | FT_DATAGRAM_LEN;
    slot->nb_bytes = 1 + varint_len(len) + len;  // Unfortunately we are always forced to account for the length field
#endif

    datagram_frame_t* frame = my_malloc_on_sending_buffer(m, cnx, sizeof(datagram_frame_t));
    if (frame == NULL) {
        //PROTOOP_PRINTF(cnx, "Unable to allocate frame structure!\n");
        my_free(cnx, slot);
        return 1;
    }
    frame->datagram_data_ptr = my_malloc_on_sending_buffer(m, cnx, (unsigned int) len);
    if (frame->datagram_data_ptr == NULL) {
        //PROTOOP_PRINTF(cnx, "Unable to allocate frame slot!\n");
        my_free(cnx, frame);
        my_free(cnx, slot);
        return 1;
    }

    frame->length = (uint64_t) len;
    while (m->send_buffer + frame->length > SEND_BUFFER) {
        free_head_datagram_reserved(m, cnx);
    }

/*
    int nb_paths = (int) get_cnx(cnx, AK_CNX_NB_PATHS, 0);
    for (uint16_t i = (uint16_t) (nb_paths > 1); i < nb_paths; i++) {
        picoquic_path_t *path = (picoquic_path_t*) get_cnx(cnx, AK_CNX_PATH, i);
        int srtt = get_path(path, AK_PATH_SMOOTHED_RTT, 0);
        if (srtt != 250000) PROTOOP_PRINTF(cnx, "Path: %d, RTT: %d\n", i, srtt);
    }
*/
    frame->datagram_id = datagram_id;
    my_memcpy(frame->datagram_data_ptr, payload, (size_t) len);
    slot->frame_ctx = frame;

    size_t reserved_size = reserve_frames(cnx, 1, slot);
    if (reserved_size < slot->nb_bytes) {
        //PROTOOP_PRINTF(cnx, "Unable to reserve frame slot\n");
        my_free(cnx, frame->datagram_data_ptr);
        my_free(cnx, frame);
        my_free(cnx, slot);
        return 1;
    }
    m->send_buffer += frame->length;


    protoop_id_t pid;
    pid.id = "timestamp_reserved";
    pid.hash = hash_value_str(pid.id);
    if (plugin_pluglet_exists(cnx, &pid, NO_PARAM, pluglet_pre) ||
		plugin_pluglet_exists(cnx, &pid, NO_PARAM, pluglet_post) ||
		plugin_pluglet_exists(cnx, &pid, NO_PARAM, pluglet_replace))
    {
       PROTOOP_PRINTF(cnx, "CALLING TIMESTAMP_RESERVED\n");
       run_noparam(cnx, pid.id, 0, NULL, NULL);
    }

    return 0;
}
