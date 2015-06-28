/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_AUXCODEC_H
#define TSC_AUXCODEC_H

#include "str.h"
#include <stdint.h>
#include <stdio.h>

/*****************************************************************************
 * Encoder                                                                   *
 *****************************************************************************/
typedef struct auxenc_t_ {
    uint32_t line_cnt; /* no. of records processed in the curr block */
    str_t* out;        /* output string (for the arithmetic coder)   */
} auxenc_t;

auxenc_t* auxenc_new(void);
void auxenc_free(auxenc_t* ae);
void auxenc_add_record(auxenc_t*      ae,
                       const char*    qname,
                       const uint64_t flag,
                       const char*    rname,
                       const uint64_t mapq,
                       const char*    rnext,
                       const uint64_t pnext,
                       const uint64_t tlen,
                       const char*    opt);
size_t auxenc_write_block(auxenc_t* auxenc, FILE* ofp);

/*****************************************************************************
 * Decoder                                                                   *
 *****************************************************************************/
typedef struct auxdec_t_ {
    uint32_t line_cnt; /* no. of records processed in the curr block */
} auxdec_t;

auxdec_t* auxdec_new(void);
void auxdec_free(auxdec_t* ad);
void auxdec_decode_block(auxdec_t* ad, FILE* ifp, str_t** aux);

#endif /* TSC_AUXCODEC_H */

