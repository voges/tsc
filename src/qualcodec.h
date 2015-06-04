/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_QUALCODEC_H
#define TSC_QUALCODEC_H

#include "cbufstr.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define QUALCODEC_WINDOW_SZ 10

/*****************************************************************************
 * Encoder                                                                   *
 *****************************************************************************/
typedef struct qualenc_t_ {
    size_t     block_sz;  /* block size (no. of SAM records)            */
    size_t     block_lc;  /* no. of records processed in the curr block */
    cbufstr_t* qual_cbuf; /* circular buffer for QUALity scores         */
    str_t*     out_buf;   /* output string (for the arithmetic coder)   */
} qualenc_t;

qualenc_t* qualenc_new(const size_t block_sz);
void qualenc_free(qualenc_t* qualenc);
void qualenc_add_record(qualenc_t* qualenc, const char* qual);
void qualenc_write_block(qualenc_t* qualenc, FILE* ofp);

/*****************************************************************************
 * Decoder                                                                   *
 *****************************************************************************/
typedef struct qualdec_t_ {
    size_t     block_sz;  /* block size (no. of SAM records)            */
    size_t     block_lc;  /* no. of records processed in the curr block */
    cbufstr_t* qual_cbuf; /* circular buffer for QUALity scores         */
} qualdec_t;

qualdec_t* qualdec_new(void);
void qualdec_free(qualdec_t* qualdec);
void qualdec_decode_block(qualdec_t* qualdec, FILE* ifp, str_t** qual);

#endif /* TSC_QUALCODEC_H */

