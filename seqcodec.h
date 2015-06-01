/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_SEQCODEC_H
#define TSC_SEQCODEC_H

#include "str.h"
#include "cbufstr.h"
#include <stdint.h>
#include <stdio.h>

#define SEQCODEC_WINDOW_SZ 10

/*****************************************************************************
 * Encoder                                                                   *
 *****************************************************************************/
typedef struct seqenc_t_ {
    size_t       block_sz;
    uint64_t     block_b;
    uint64_t*    pos_buf;
    str_t**      cigar_buf;
    str_t**      seq_buf;
    cbufstr_t*   exp_cbuf;
    unsigned int buf_pos;
    str_t*       out;
} seqenc_t;

seqenc_t* seqenc_new(const size_t block_sz);
void seqenc_free(seqenc_t* seqenc);
void seqenc_add_record(seqenc_t* seqenc, uint64_t pos, const char* cigar, const char* seq);
void seqenc_write_block(seqenc_t* seqenc, FILE* fp);

/*****************************************************************************
 * Decoder                                                                   *
 *****************************************************************************/
typedef struct seqdec_t_ {
    size_t       block_sz;
    uint64_t*    pos_buf;
    str_t**      cigar_buf;
    str_t**      seq_buf;
    unsigned int buf_pos;
} seqdec_t;

seqdec_t* seqdec_new(void);
void seqdec_free(seqdec_t* seqdec);
void seqdec_decode_block(seqdec_t* seqenc, const uint64_t block_nb);

#endif /* TSC_SEQCODEC_H */

