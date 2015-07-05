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
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define QUALCODEC_WINDOW_SZ 100
#define QUALCODEC_MAX 126
#define QUALCODEC_MIN 33
#define QUALCODEC_RANGE (QUALCODEC_MAX-QUALCODEC_MIN)
#define QUALCODEC_TABLE_SZ (QUALCODEC_RANGE*QUALCODEC_RANGE*QUALCODEC_RANGE)
#define QUALCODEC_MEM_SZ 3

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
typedef struct qualenc_t_ {
    size_t     blkl_n;    /* no. of records processed in the curr block */
    cbufstr_t* qual_cbuf; /* circular buffer for QUALity scores         */
    str_t*     residues;  /* output string (for the arithmetic coder)   */

    /* These are only needed for o2 compression ("line context"). */
    cbufint64_t* qual_cbuf_len;
    cbufint64_t* qual_cbuf_mu;
    cbufint64_t* qual_cbuf_var;
    unsigned int freq[QUALCODEC_TABLE_SZ][QUALCODEC_RANGE]; /* 300 MiB */
    char         pred[QUALCODEC_TABLE_SZ];
} qualenc_t;

qualenc_t* qualenc_new(void);
void qualenc_free(qualenc_t* qualenc);
void qualenc_add_record(qualenc_t* qualenc, const char* qual);
size_t qualenc_write_block(qualenc_t* qualenc, FILE* ofp);

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
typedef struct qualdec_t_ {

} qualdec_t;

qualdec_t* qualdec_new(void);
void qualdec_free(qualdec_t* qualdec);
void qualdec_decode_block(qualdec_t* qualdec, FILE* ifp, str_t** qual);

#endif /* TSC_QUALCODEC_H */

