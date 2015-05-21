/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "seqenc.h"
#include "tsclib.h"
#include <string.h>

static void seqenc_init(seqenc_t* seqenc)
{
    seqenc->buf_pos = 0;
    seqenc->block_nb = 0;
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
    if (seqenc->buf_pos < seqenc->block_sz) {
        seqenc->pos_buf[seqenc->buf_pos] = pos;
        str_copy_cstr(seqenc->cigar_buf[seqenc->buf_pos], cigar);
        str_copy_cstr(seqenc->seq_buf[seqenc->buf_pos], seq);
        seqenc->buf_pos++;
    } else {
        tsc_error("Block buffer overflow.");
    }
}

void seqenc_write_block(seqenc_t* seqenc, fwriter_t* fwriter)
{
    DEBUG("Writing block ...");

    /* Write block header */
    fwriter_write_cstr(fwriter, "NUC");              /*< this is a nucleotide block     */
    fwriter_write_uint64(fwriter, seqenc->block_nb); /*< total number of bytes in block */

    unsigned int i = 0;
    for (i = 0; i < seqenc->buf_pos; i++) {
        fwriter_write_uint64(fwriter, seqenc->pos_buf[i]);
        fwriter_write_cstr(fwriter, "\t");
        fwriter_write_cstr(fwriter, seqenc->cigar_buf[i]->s);
        fwriter_write_cstr(fwriter, "\t");
        fwriter_write_cstr(fwriter, seqenc->seq_buf[i]->s);
        fwriter_write_cstr(fwriter, "\n");
    }
    seqenc->buf_pos = 0;
}

