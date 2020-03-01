#include "../bpf.h"

//This function has two outputs. It sets the bpfd->last_path_index_sent to the path_index
//It also returns the path that is being used to send

//FoC:
//For each path:
//If the path is not active: skip
//If the path has not been verified, the challenge timed out and too many have not been sent: send challenge
//If a challenge needs to be responded to: send response
//If a path is verified then do not send on AK_CNX_PATH (the control path)
//If the congestion window is full then don't send on path
//If a PING is received: send PONG
//If MTU on path is unknown: probe for MTU
//Select appropriate path according to whatever metric
//Path is selected by setting bpfd->last_path_index_sent to the path index and returning the path
//Is not allowed to return NULL
protoop_arg_t schedule_path_rr(picoquic_cnx_t *cnx) {
	//Sets up variables
	//path_x is the chosen path that we return
    picoquic_path_t *path_x = (picoquic_path_t *) get_cnx(cnx, AK_CNX_PATH, 0);
	//Don't know. Maybe the management data path?
    picoquic_path_t *path_0 = path_x;
	//Chosen path
    picoquic_path_t *path_c = NULL;
	//Help data that is used to get data about paths
    bpf_data *bpfd = get_bpf_data(cnx);
	//Data about a specific path
    path_data_t *pd = NULL;
	//The selected path, similar to path_x but stored in the VM
    uint8_t selected_path_index = 255;
	//Don't know
    manage_paths(cnx);
	//Current time, used to figure out timeout and so on
    uint64_t now = picoquic_current_time();
	//Don't know
	//Reason for sending on this path?
    char *path_reason = "";

    PROTOOP_PRINTF(cnx, "PRIORITY SCHEDULER: deciding what path to send on\n");
    for (int i = 0; i < bpfd->nb_sending_proposed; i++) {
        pd = bpfd->sending_paths[i];
        PROTOOP_PRINTF(cnx, "PRIORITY SCHEDULER: exploring path, i: %d\n", i);

        /* A (very) simple round-robin */
		//If the path is not active then skip it
        if (pd->state == path_active) {
            path_c = pd->path;
            int challenge_verified_c = (int) get_path(path_c, AK_PATH_CHALLENGE_VERIFIED, 0);
            uint64_t challenge_time_c = (uint64_t) get_path(path_c, AK_PATH_CHALLENGE_TIME, 0);
            uint64_t retransmit_timer_c = (uint64_t) get_path(path_c, AK_PATH_RETRANSMIT_TIMER, 0);
            uint8_t challenge_repeat_count_c = (uint8_t) get_path(path_c, AK_PATH_CHALLENGE_REPEAT_COUNT, 0);

            uint64_t c_w = (uint64_t) get_path(path_c, AK_PATH_CWIN, 0);
            uint64_t b_i_t = (uint64_t) get_path(path_c, AK_PATH_BYTES_IN_TRANSIT, 0);
            //picoquic_packet_t* packet = (picoquic_packet_t*) get_cnx(cnx, AK_CNX_INPUT, 0);
            //uint32_t pkt_len = packet->length;
            PROTOOP_PRINTF(cnx, "PRIORITY SCHEDULER: Path %d: cwin: %" PRIu64 ", bytes_in_transit: %" PRIu64", packet_len: %" PRIu64 "\n", i, c_w, b_i_t, /*pkt_len*/);
         }
    }


    PROTOOP_PRINTF(cnx, "PRIORITY SCHEDULER: deciding what path to send on\n");
    for (int i = 0; i < bpfd->nb_sending_proposed; i++) {
    //for (int i = 0; i < 8; i++) {
        pd = bpfd->sending_paths[i];
        PROTOOP_PRINTF(cnx, "PRIORITY SCHEDULER: exploring path, i: %d\n", i);

        /* A (very) simple round-robin */
		//If the path is not active then skip it
        if (pd->state == path_active) {
            path_c = pd->path;
            int challenge_verified_c = (int) get_path(path_c, AK_PATH_CHALLENGE_VERIFIED, 0);
            uint64_t challenge_time_c = (uint64_t) get_path(path_c, AK_PATH_CHALLENGE_TIME, 0);
            uint64_t retransmit_timer_c = (uint64_t) get_path(path_c, AK_PATH_RETRANSMIT_TIMER, 0);
            uint8_t challenge_repeat_count_c = (uint8_t) get_path(path_c, AK_PATH_CHALLENGE_REPEAT_COUNT, 0);

            uint64_t c_w = (uint64_t) get_path(path_c, AK_PATH_CWIN, 0);
            uint64_t b_i_t = (uint64_t) get_path(path_c, AK_PATH_BYTES_IN_TRANSIT, 0);
		PROTOOP_PRINTF(cnx, "PRIORITY SCHEDULER: cwin: %" PRIu64 ", bytes_in_transit: %" PRIu64 "\n", c_w, b_i_t);
			//If the challenge is not verified and the retransmit timer has expired and we haven't sent enough challenges
			//Send a challenge
            if (!challenge_verified_c && challenge_time_c + retransmit_timer_c < now && challenge_repeat_count_c < PICOQUIC_CHALLENGE_REPEAT_MAX) {
                /* Start the challenge! */
                path_x = path_c;
                selected_path_index = i;
                path_reason = "CHALLENGE_REQUEST";
                break;
            }

            int challenge_response_to_send_c = (int) get_path(path_c, AK_PATH_CHALLENGE_RESPONSE_TO_SEND, 0);
			//If there is a challenge response that is avaliable to send
			//Send a challenge response
            if (challenge_response_to_send_c) {
                /* Reply as soon as possible! */
                path_x = path_c;
                selected_path_index = i;
                path_reason = "CHALLENGE_RESPONSE";
                break;
            }

            /* At this point, this means path 0 should NEVER be reused anymore! */
			//Avoid sending on the communication path
            uint64_t pkt_sent_c = (uint64_t) get_path(path_c, AK_PATH_NB_PKT_SENT, 0);
            if (challenge_verified_c && path_x == path_0) {
                path_x = path_c;
                selected_path_index = i;
                path_reason = "AVOID_PATH_0";
            }

            /* Very important: don't go further if the cwin is exceeded! */
            uint64_t cwin_c = (uint64_t) get_path(path_c, AK_PATH_CWIN, 0);
            uint64_t bytes_in_transit_c = (uint64_t) get_path(path_c, AK_PATH_BYTES_IN_TRANSIT, 0);
            cwin_c = 80;
            //picoquic_packet_t* packet = (picoquic_packet_t*) get_cnx(cnx, AK_CNX_INPUT, 0);
            //uint32_t pkt_len = packet->length;
            //Make sure we don't send more than congestion window
            //if (cwin_c <= bytes_in_transit_c) {
            if (cwin_c <= bytes_in_transit_c) {
                PROTOOP_PRINTF(cnx, "PRIORITY SCHEDULER: CONGESTED PATH, SKIPPING %d\n", i);
                continue;
            }

            int ping_received_c = (int) get_path(path_c, AK_PATH_PING_RECEIVED, 0);
            //On ping always return a pong on the pinged path
            if (ping_received_c) {
                /* We need some action from the path! */
                path_x = path_c;
                selected_path_index = i;
                path_reason = "PONG";
                break;
            }

            int mtu_needed = (int) helper_is_mtu_probe_needed(cnx, path_c);
            //If we don't know the MTU for the path then we probe for it
            if (mtu_needed) {
                path_x = path_c;
                selected_path_index = i;
                path_reason = "MTU_DISCOVERY";
                break;
            }

            /* Don't consider invalid paths */
			//Don't use unverified paths
            if (!challenge_verified_c) {
                PROTOOP_PRINTF(cnx, "PRIORITY SCHEDULER: Challenge is not verified, continuing\n");
                continue;
            }

            path_x = pd->path;
            selected_path_index = i;
            path_reason = "PRIORITY SCHEDULING";
            PROTOOP_PRINTF(cnx, "PRIORITY SCHEDULER: SENT ON PATH index: %d, id: %" PRIu64 "\n", i, pd->path_id);
            break;
        }
    }

    bpfd->last_path_index_sent = selected_path_index;
    LOG {
        size_t path_reason_len = strlen(path_reason) + 1;
        char *p_path_reason = my_malloc(cnx, path_reason_len);
        my_memcpy(p_path_reason, path_reason, path_reason_len);
        LOG_EVENT(cnx, "MULTIPATH", "SCHEDULE_PATH", p_path_reason, "{\"path\": \"%p\"}", (protoop_arg_t) path_x);
        my_free(cnx, p_path_reason);
    }
    return (protoop_arg_t) path_x;
}
