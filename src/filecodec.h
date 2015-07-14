/*
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>
 *
 * This file is part of tsc.
 */

#ifndef TSC_FILECODEC_H
#define TSC_FILECODEC_H

#include "./sub-block_codecs/auxcodec.h"
#include "./sub-block_codecs/nuccodec.h"
#include "./sub-block_codecs/qualcodec.h"
#include "samparser.h"
#include "./str/str.h"
#include <stdio.h>

#define FILECODEC_BLK_LC 10000LL /* no. of SAM lines per block */

/*
 * Encoder
 */
typedef struct fileenc_t_ {
    FILE*        ifp;
    FILE*        ofp;
    samparser_t* samparser;
    auxenc_t*    auxenc;
    nucenc_t*    nucenc;
    qualenc_t*   qualenc;
} fileenc_t;

fileenc_t* fileenc_new(FILE* ifp, FILE* ofp);
void fileenc_free(fileenc_t* fileenc);
void fileenc_encode(fileenc_t* fileenc);

/*
 * Decoder
 */
typedef struct filedec_t_ {
    FILE*      ifp;
    FILE*      ofp;
    auxdec_t*  auxdec;
    nucdec_t*  nucdec;
    qualdec_t* qualdec;
} filedec_t;

filedec_t* filedec_new(FILE* ifp, FILE* ofp);
void filedec_free(filedec_t* filedec);
void filedec_decode(filedec_t* filedec);
void filedec_info(filedec_t* filedec);

#endif /* TSC_FILECODEC_H */

