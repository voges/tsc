/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_SEQENC_H
#define TSC_SEQENC_H

#include "str.h"
#include "fwriter.h"
#include <stdint.h>

typedef struct seqenc_t_ {
    unsigned int block_sz;
    uint64_t block_nb;
    uint64_t* pos_buf;
    str_t** cigar_buf;
    str_t** seq_buf;
    unsigned int buf_pos;
} seqenc_t;

seqenc_t* seqenc_new(unsigned int block_sz);
void seqenc_free(seqenc_t* seqenc);
void seqenc_add_record(seqenc_t* seqenc, uint64_t pos, const char* cigar, const char* seq);
void seqenc_write_block(seqenc_t* seqenc, fwriter_t* fwriter);

#endif // TSC_SEQENC_H

