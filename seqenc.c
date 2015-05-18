/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "seqenc.h"
#include "tsclib.h"
#include <string.h>

static void seqenc_init(seqenc_t* seqenc)
{
    seqenc->buf_pos = 0;
}

seqenc_t* seqenc_new(unsigned int block_sz)
{
    seqenc_t* seqenc = (seqenc_t*)tsc_malloc_or_die(sizeof(seqenc_t));
    seqenc->block_sz = block_sz;
    seqenc->pos_buf = (uint64_t*)tsc_malloc_or_die(sizeof(uint64_t) * block_sz);
    seqenc->cigar_buf = (str_t**)tsc_malloc_or_die(sizeof(str_t*) * block_sz);
    seqenc->seq_buf = (str_t**)tsc_malloc_or_die(sizeof(str_t*) * block_sz);
    unsigned int i = 0;
    for (i = 0; i < block_sz; i++) {
        seqenc->cigar_buf[i] = str_new();
        seqenc->seq_buf[i] = str_new();
    }
    seqenc_init(seqenc);
    return seqenc;
}

void seqenc_free(seqenc_t* seqenc)
{
    if (seqenc != NULL) {
        unsigned int i = 0;
        for (i = 0; i < seqenc->block_sz; i++) {
            str_free(seqenc->cigar_buf[i]);
            str_free(seqenc->seq_buf[i]);
        }
        free((void*)seqenc->pos_buf);
        free((void*)seqenc->cigar_buf);
        free((void*)seqenc->seq_buf);
        free((void*)seqenc);
        seqenc = NULL;
    } else { /* fileenc == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

void seqenc_add_record(seqenc_t* seqenc, uint64_t pos, const char *cigar, const char *seq)
{
    seqenc->pos_buf[seqenc->buf_pos] = pos;
    DEBUG("pos=%d", pos);
    str_append_cstr(seqenc->cigar_buf[seqenc->buf_pos], cigar);

    if (seqenc->buf_pos > 1) {
        uint64_t pos_off = seqenc->pos_buf[seqenc->buf_pos] - seqenc->pos_buf[seqenc->buf_pos - 1];
        seqenc->pos_buf[seqenc->buf_pos] = pos;
        seq += strlen(seq) - pos_off;
        DEBUG("pos_off=%d, seq=%s", pos_off,seq);
        str_append_cstr(seqenc->seq_buf[seqenc->buf_pos], seq);
        str_append_cstr(seqenc->seq_buf[seqenc->buf_pos], "\n");
    } else {
        str_append_cstr(seqenc->seq_buf[seqenc->buf_pos], seq);
        str_append_cstr(seqenc->seq_buf[seqenc->buf_pos], "\n");
    }

    seqenc->buf_pos++;
}

void seqenc_output_records(seqenc_t* seqenc, str_t* out)
{
    DEBUG("Flushing buffer, outputting records.");

    unsigned int i = 0;
    for (i = 0; i < seqenc->block_sz; i++) {
        str_append_str(out, seqenc->seq_buf[i]);
        str_append_cstr(out, "\n");
        seqenc->pos_buf[i] = 0;
        str_clear(seqenc->cigar_buf[i]);
        str_clear(seqenc->seq_buf[i]);
    }

    seqenc->buf_pos = 0;
}

