/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#ifndef TSC_SEQCODEC_H
#define TSC_SEQCODEC_H

#include "cbufint64.h"
#include "cbufstr.h"
#include "str.h"
#include <stdint.h>
#include <stdio.h>

#define SEQCODEC_WINDOW_SZ 10

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
typedef struct seqenc_t_ {
    size_t         block_sz;   /* block size (no. of SAM records)            */
    size_t         block_lc;   /* no. of records processed in the curr block */
    cbufint64_t*   pos_cbuf;   /* circular buffer for POSitions              */
    cbufstr_t*     cigar_cbuf; /* circular buffer for CIGARs                 */
    cbufstr_t*     seq_cbuf;   /* circular buffer for SEQuences              */
    cbufstr_t*     exp_cbuf;   /* circular buffer for EXPanded sequences     */
    str_t*         out_buf;    /* output string (for the arithmetic coder)   */
} seqenc_t;

seqenc_t* seqenc_new(const size_t block_sz);
void seqenc_free(seqenc_t* seqenc);
void seqenc_add_record(seqenc_t* seqenc, uint64_t pos, const char* cigar, const char* seq);
void seqenc_write_block(seqenc_t* seqenc, FILE* ofp);

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
typedef struct seqdec_t_ {
    size_t       block_sz;   /* block size (no. of SAM records)            */
    size_t       block_lc;   /* no. of records processed in the curr block */
    cbufint64_t* pos_cbuf;   /* circular buffer for POSitions              */
    cbufstr_t*   cigar_cbuf; /* circular buffer for CIGARs                 */
    cbufstr_t*   seq_cbuf;   /* circular buffer for SEQuences              */
    cbufstr_t*   exp_cbuf;   /* circular buffer for EXPanded sequences     */
} seqdec_t;

seqdec_t* seqdec_new(void);
void seqdec_free(seqdec_t* seqdec);
void seqdec_decode_block(seqdec_t* seqdec, FILE* ifp, str_t** seq);

#endif /* TSC_SEQCODEC_H */

