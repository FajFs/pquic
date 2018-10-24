#include "picoquic_internal.h"
#include "plugin.h"
#include "util.h"
#include "../helpers.h"
#include "bpf.h"


/**
 * See PROTOOP_NOPARAM_DECODE_STREAM_FRAME
 */
protoop_arg_t check_spurious_stream_frame(picoquic_cnx_t *cnx)
{
    uint8_t *bytes = (uint8_t *) cnx->protoop_inputv[0];
    const uint8_t *bytes_end = (const uint8_t *) cnx->protoop_inputv[1];
    picoquic_path_t *path = (picoquic_path_t *) cnx->protoop_inputv[3];

    uint64_t stream_id;
    uint64_t offset;
    size_t data_length;
    int fin;
    size_t consumed;

    int ret = helper_parse_stream_header(bytes, (size_t)(bytes_end - bytes), (protoop_arg_t*[]){&stream_id, &offset, &data_length, (protoop_arg_t *) &fin, &consumed});
    if (ret == 0) {
        picoquic_stream_head *stream = picoquic_find_stream(cnx, stream_id, false);
        uint64_t consumed_offset = stream == NULL ? 0 : stream->consumed_offset;
        cop2_path_metrics *path_metrics = find_metrics_for_path(cnx, get_cop2_metrics(cnx), path);
        if(offset + data_length < consumed_offset) {  // We already received the whole segment
            path_metrics->metrics.data_dupl += data_length;
            path_metrics->metrics.pkt_dupl++;
        } else if (offset < consumed_offset) {  // We already received a part of the segment
            path_metrics->metrics.data_dupl += data_length - (consumed_offset - offset);
            path_metrics->metrics.pkt_dupl++;
        }
    }

    return 0;
}