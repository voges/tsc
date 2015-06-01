/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_QUALCODEC_H
#define TSC_QUALCODEC_H

#include "cbufstr.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define QUALCODEC_WINDOW_SZ 10

/*****************************************************************************
 * Encoder                                                                   *
 *****************************************************************************/
typedef struct qualenc_t_ {
    uint64_t block_b;
    size_t block_sz;
    size_t window_sz;
    cbufstr_t* qual_buf;
    unsigned char* out_buf;
} qualenc_t;

qualenc_t* qualenc_new(const size_t block_sz);
void qualenc_free(qualenc_t* qualenc);
void qualenc_add_record(qualenc_t* qualenc, const char* qual);
void qualenc_write_block(qualenc_t* qualenc, FILE* fp);

/*****************************************************************************
 * Decoder                                                                   *
 *****************************************************************************/
typedef struct qualdec_t_ {

} qualdec_t;

qualdec_t* qualdec_new(void);
void qualdec_free(qualdec_t* qualdec);
void qualdec_decode_block(qualdec_t* qualdec, const uint64_t block_nb);

#endif /* TSC_QUALCODEC_H */

