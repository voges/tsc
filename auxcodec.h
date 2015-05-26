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
#include "fwriter.h"

/*****************************************************************************
 * Encoder                                                                   *
 *****************************************************************************/
typedef struct auxenc_t_ {
    unsigned int block_sz;
    str_t** aux_buf;
    unsigned int buf_pos;
} auxenc_t;

auxenc_t* auxenc_new(unsigned int block_sz);
void auxenc_free(auxenc_t* auxenc);
void auxenc_add_record(auxenc_t* auxenc,
                       const char* qname,
                       uint64_t flag,
                       const char *rname,
                       uint64_t mapq,
                       const char *rnext,
                       uint64_t pnext,
                       uint64_t tlen,
                       const char *opt);
void auxenc_write_block(auxenc_t* auxenc, fwriter_t* fwriter);

/*****************************************************************************
 * Decoder                                                                   *
 *****************************************************************************/
typedef struct auxdec_t_ {
    unsigned int block_sz;
    str_t** aux_buf;
    unsigned int buf_pos;
} auxdec_t;

auxdec_t* auxdec_new(void);
void auxdec_free(auxdec_t* auxdec);

#endif // TSC_AUXCODEC_H

