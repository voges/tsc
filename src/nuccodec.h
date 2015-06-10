/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#ifndef TSC_NUCCODEC_H
#define TSC_NUCCODEC_H

#include "cbufint64.h"
#include "cbufstr.h"
#include "str.h"
#include <stdint.h>
#include <stdio.h>

#define NUCCODEC_WINDOW_SZ 10

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
typedef struct nucenc_t_ {
    uint32_t       block_lc;   /* no. of records processed in the curr block */
    cbufint64_t*   pos_cbuf;   /* circular buffer for POSitions              */
    cbufstr_t*     cigar_cbuf; /* circular buffer for CIGARs                 */
    cbufstr_t*     seq_cbuf;   /* circular buffer for SEQuences              */
    cbufstr_t*     exp_cbuf;   /* circular buffer for EXPanded sequences     */
    str_t*         out_buf;    /* output string (for the arithmetic coder)   */
} nucenc_t;

nucenc_t* nucenc_new(void);
void nucenc_free(nucenc_t* nucenc);
void nucenc_add_record(nucenc_t* nucenc, uint64_t pos, const char* cigar, const char* seq);
size_t nucenc_write_block(nucenc_t* nucenc, FILE* ofp);

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
typedef struct nucdec_t_ {
    uint32_t     block_lc;   /* no. of records processed in the curr block */
    cbufint64_t* pos_cbuf;   /* circular buffer for POSitions              */
    cbufstr_t*   cigar_cbuf; /* circular buffer for CIGARs                 */
    cbufstr_t*   seq_cbuf;   /* circular buffer for SEQuences              */
    cbufstr_t*   exp_cbuf;   /* circular buffer for EXPanded sequences     */
} nucdec_t;

nucdec_t* nucdec_new(void);
void nucdec_free(nucdec_t* nucdec);
void nucdec_decode_block(nucdec_t* nucdec, FILE* ifp, uint64_t* pos, str_t** cigar, str_t** seq);

#endif /* TSC_NUCCODEC_H */

