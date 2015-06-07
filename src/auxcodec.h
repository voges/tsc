/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_AUXCODEC_H
#define TSC_AUXCODEC_H

#include "str.h"
#include <stdio.h>
#include <stdint.h>

/*****************************************************************************
 * Encoder                                                                   *
 *****************************************************************************/
typedef struct auxenc_t_ {
    size_t  block_sz; /* block size (no. of SAM records)            */
    size_t  block_lc; /* no. of records processed in the curr block */
    str_t*  out_buf;  /* buffer holding the delimited aux records   */
} auxenc_t;

auxenc_t* auxenc_new(const size_t block_sz);
void auxenc_free(auxenc_t* auxenc);
void auxenc_add_record(auxenc_t*   auxenc,
                       const char* qname,
                       uint64_t    flag,
                       const char* rname,
                       uint64_t    mapq,
                       const char* rnext,
                       uint64_t    pnext,
                       uint64_t    tlen,
                       const char* opt);
size_t auxenc_write_block(auxenc_t* auxenc, FILE* ofp);

/*****************************************************************************
 * Decoder                                                                   *
 *****************************************************************************/
typedef struct auxdec_t_ {
    size_t  block_sz; /* block size (no. of SAM records)              */
    size_t  block_lc; /* no. of records processed in the curr block   */
} auxdec_t;

auxdec_t* auxdec_new(void);
void auxdec_free(auxdec_t* auxdec);
void auxdec_decode_block(auxdec_t* auxdec, FILE* ifp, str_t** aux);

#endif // TSC_AUXCODEC_H

