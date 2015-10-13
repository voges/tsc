/*
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>
 *
 * This file is part of tsc.
 */

#ifndef TSC_AUXCODEC_H
#define TSC_AUXCODEC_H

#include "../str/str.h"
#include <stdint.h>
#include <stdio.h>

/*
 * Encoder
 * -----------------------------------------------------------------------------
 */

typedef struct auxenc_t_ {
    size_t blkl_n;   /* no. of records processed in the curr block        */
    str_t* residues; /* prediction residues (passed to the entropy coder) */
} auxenc_t;

auxenc_t* auxenc_new(void);
void auxenc_free(auxenc_t* auxenc);
void auxenc_add_record(auxenc_t*      auxenc,
                       const char*    qname,
                       const int64_t  flag,
                       const char*    rname,
                       const int64_t  mapq,
                       const char*    rnext,
                       const int64_t  pnext,
                       const int64_t  tlen,
                       const char*    opt);
size_t auxenc_write_block(auxenc_t* auxenc, FILE* ofp);

/*
 * Decoder
 * -----------------------------------------------------------------------------
 */

typedef struct auxdec_t_ {

} auxdec_t;

auxdec_t* auxdec_new(void);
void auxdec_free(auxdec_t* auxdec);
void auxdec_decode_block(auxdec_t* auxdec,
                         FILE*     ifp,
                         str_t**   qname,
                         int64_t*  flag,
                         str_t**   rname,
                         int64_t*  mapq,
                         str_t**   rnext,
                         int64_t*  pnext,
                         int64_t*  tlen,
                         str_t**   opt);

#endif /* TSC_AUXCODEC_H */

