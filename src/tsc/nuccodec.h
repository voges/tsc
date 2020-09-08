// Copyright 2015 Leibniz University Hannover (LUH)

#ifndef TSC_NUCCODEC_H_
#define TSC_NUCCODEC_H_

#define TSC_NUCCODEC_WINDOW_SIZE 10

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cbufint64.h"
#include "cbufstr.h"
#include "str.h"

typedef struct nuccodec_t_ {
    // Only used in encoder
    size_t record_cnt;
    bool first;
    str_t *rname_prev;
    uint32_t pos_prev;

    // Statistics
    size_t mrecord_cnt;
    size_t irecord_cnt;
    size_t precord_cnt;
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
    cbufstr_t *exs_cbuf;
    str_t *ref;
    uint32_t ref_pos_min;
    uint32_t ref_pos_max;
} nuccodec_t;

nuccodec_t *nuccodec_new();

void nuccodec_free(nuccodec_t *nuccodec);

// Encoder
// -----------------------------------------------------------------------------

void nuccodec_add_record(nuccodec_t *nuccodec, const char *rname, uint32_t pos, const char *cigar, const char *seq);

size_t nuccodec_write_block(nuccodec_t *nuccodec, FILE *fp);

// Decoder methods
// -----------------------------------------------------------------------------

size_t nuccodec_decode_block(nuccodec_t *nuccodec, FILE *fp, str_t **rname, uint32_t *pos, str_t **cigar, str_t **seq);

#endif  // TSC_NUCCODEC_H_
