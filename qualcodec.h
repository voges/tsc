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

/*****************************************************************************
 * Encoder                                                                   *
 *****************************************************************************/
typedef struct qualenc_t_ {
    uint64_t block_nb;
    unsigned int block_sz;
    unsigned int window_sz;
    cbufstr_t* qual_buf;
    unsigned char* out_buf;
} qualenc_t;

qualenc_t* qualenc_new(const unsigned int block_sz, const unsigned int window_sz);
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

#endif /* TSC_QUALCODEC_H */

