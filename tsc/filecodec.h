/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#ifndef TSC_FILECODEC_H
#define TSC_FILECODEC_H

#include "auxcodec.h"
#include "nuccodec.h"
#include "qualcodec.h"
#include "samparser.h"
#include "str.h"
#include <stdio.h>

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
typedef struct fileenc_t_ {
    FILE*        ifp;
    FILE*        ofp;
    size_t       block_sz;
    samparser_t* samparser;
    auxenc_t*    auxenc;
    nucenc_t*    nucenc;
    qualenc_t*   qualenc;
    str_t*       stats;
} fileenc_t;

fileenc_t* fileenc_new(FILE* ifp, FILE* ofp, const size_t block_sz);
void fileenc_free(fileenc_t* fileenc);
str_t* fileenc_encode(fileenc_t* fileenc);

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
typedef struct filedec_t_ {
    FILE*      ifp;
    FILE*      ofp;
    auxdec_t*  auxdec;
    nucdec_t*  nucdec;
    qualdec_t* qualdec;
    str_t*     stats;
} filedec_t;

filedec_t* filedec_new(FILE* ifp, FILE* ofp);
void filedec_free(filedec_t* filedec);
str_t* filedec_decode(filedec_t* filedec);

#endif /*TSC_FILECODEC_H */

