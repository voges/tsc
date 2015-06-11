/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#ifndef TSC_QUALCODEC_H
#define TSC_QUALCODEC_H

#include "cbufint64.h"
#include "cbufstr.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define QUALCODEC_WINDOW_SZ 10

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
typedef struct qualenc_t_ {
    unsigned int order;     /* order of compression                       */
    uint32_t     block_lc;  /* no. of records processed in the curr block */
    cbufstr_t*   qual_cbuf; /* circular buffer for QUALity scores         */
    str_t*       out_buf;   /* output string (for the arithmetic coder)   */

    /* These are only needed for o2 compression ("line context"). */
    cbufint64_t* qual_cbuf_len;
    cbufint64_t* qual_cbuf_mu;
    cbufint64_t* qual_cbuf_var;
    unsigned int freq[256 * 256][256]; /* 16 MiB */
    char         pred[256];
} qualenc_t;

qualenc_t* qualenc_new(const unsigned int order);
void qualenc_free(qualenc_t* qualenc);
void qualenc_add_record(qualenc_t* qualenc, const char* qual);
size_t qualenc_write_block(qualenc_t* qualenc, FILE* ofp);

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
typedef struct qualdec_t_ {
    uint32_t   block_lc;  /* no. of records processed in the curr block */
    cbufstr_t* qual_cbuf; /* circular buffer for QUALity scores         */
} qualdec_t;

qualdec_t* qualdec_new(void);
void qualdec_free(qualdec_t* qualdec);
void qualdec_decode_block(qualdec_t* qualdec, FILE* ifp, str_t** qual);

#endif /* TSC_QUALCODEC_H */

