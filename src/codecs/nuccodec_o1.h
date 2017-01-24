#ifndef TSC_NUCCODEC_O1_H
#define TSC_NUCCODEC_O1_H

#define TSC_NUCCODEC_O1_WINDOW_SIZE 10

#include "support/cbufint64.h"
#include "support/cbufstr.h"
#include "tsclib/str.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct nuccodec_t_ {
    // Only used in encoder
    size_t  record_cnt;  // No. of records processed in the current block
    bool    first;       // 'true', if first line has not been processed yet
    str_t   *rname_prev; // Previous RNAME
    uint32_t pos_prev;   // Previous POSition

    // Statistics
    size_t mrecord_cnt; // Total no. of M-Records processed
    size_t irecord_cnt; // Total no. of I-Records processed
    size_t precord_cnt; // Total no. of P-Records processed
    size_t ctrl_sz;
    size_t rname_sz;
    size_t pos_sz;
    size_t seq_sz;
    size_t seqlen_sz;
    size_t exs_sz;
    size_t posoff_sz;
    size_t stogy_sz;
    size_t inserts_sz;
    size_t modcnt_sz;
    size_t modpos_sz;
    size_t modbases_sz;
    size_t trail_sz;

    // Compressed streams
    str_t *ctrl;
    str_t *rname;
    str_t *pos;
    str_t *seq;
    str_t *seqlen;
    str_t *exs;
    str_t *posoff;
    str_t *stogy;
    str_t *inserts;
    str_t *modcnt;
    str_t *modpos;
    str_t *modbases;
    str_t *trail;

    // Sliding window
    cbufint64_t *pos_cbuf;
    cbufstr_t   *exs_cbuf;
    str_t       *ref;
    uint32_t    ref_pos_min;
    uint32_t    ref_pos_max;
} nuccodec_t;

nuccodec_t * nuccodec_new(void);
void nuccodec_free(nuccodec_t *nuccodec);

// Encoder
// -----------------------------------------------------------------------------

void nuccodec_add_record(nuccodec_t     *nuccodec,
                         //const uint16_t flag,
                         const char     *rname,
                         const uint32_t pos,
                         const char     *cigar,
                         const char     *seq);
size_t nuccodec_write_block(nuccodec_t *nuccodec, FILE *fp);

// Decoder methods
// -----------------------------------------------------------------------------

size_t nuccodec_decode_block(nuccodec_t *nuccodec,
                             FILE       *fp,
                             str_t      **rname,
                             uint32_t   *pos,
                             str_t      **cigar,
                             str_t      **seq);

#endif // TSC_NUCCODEC_O1_H

