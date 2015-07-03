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
    size_t       blk_lc; /* number of lines in a block */
    samparser_t* samparser;

    /* Encoders. */
    auxenc_t*    auxenc;
    nucenc_t*    nucenc;
    qualenc_t*   qualenc;

    /* Input statistics. */
    size_t in_sz[12];
    enum {
        IN_QNAME,
        IN_FLAG,
        IN_RNAME,
        IN_POS,
        IN_MAPQ,
        IN_CIGAR,
        IN_RNEXT,
        IN_PNEXT,
        IN_TLEN,
        IN_SEQ,
        IN_QUAL,
        IN_OPT
    };

    /* Output statistics. */
    size_t out_sz[6];
    enum {
        OUT_TOTAL, /* total no. of bytes written                 */
        OUT_FF,    /* total no. of bytes written for file format */
        OUT_SH,    /* total no. of bytes written for SAM header  */
        OUT_AUX,   /* total no. of bytes written by auxenc       */
        OUT_NUC,   /* total no. of bytes written by nucenc       */
        OUT_QUAL   /* total no. of bytes written by qualenc      */
    };

    /* Timing statistics. */
    long elapsed[7];
    enum {
        ELAPSED_TOTAL,    /* time elapsed for file compression             */
        ELAPSED_AUXPRED,  /* time elapsed for predictive aux coding        */
        ELAPSED_NUCPRED,  /* time elapsed for predictive aux coding        */
        ELAPSED_QUALPRED, /* time elapsed for predictive aux coding        */
        ELAPSED_AUXENTR,  /* time elapsed for entropy coding of aux blocks */
        ELAPSED_NUCENTR,  /* time elapsed for entropy coding of aux blocks */
        ELAPSED_QUALENTR  /* time elapsed for entropy coding of aux blocks */
    };
} fileenc_t;

fileenc_t* fileenc_new(FILE* ifp, FILE* ofp, const size_t blk_lc);
void fileenc_free(fileenc_t* fileenc);
void fileenc_encode(fileenc_t* fileenc);

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
typedef struct filedec_t_ {
    FILE*      ifp;
    FILE*      ofp;
    auxdec_t*  auxdec;
    nucdec_t*  nucdec;
    qualdec_t* qualdec;
    size_t     out_sz;
} filedec_t;

filedec_t* filedec_new(FILE* ifp, FILE* ofp);
void filedec_free(filedec_t* filedec);
void filedec_decode(filedec_t* filedec);
void filedec_info(filedec_t* filedec);

#endif /*TSC_FILECODEC_H */

