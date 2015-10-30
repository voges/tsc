/*
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>
 *
 * This file is part of tsc.
 */

#ifndef TSC_NUCCODEC_H
#define TSC_NUCCODEC_H

//#define NUCCODEC_O0
#define NUCCODEC_O1

#include "../tvclib/str.h"
#include <stdint.h>
#include <stdio.h>

#ifdef NUCCODEC_O0

/* #include <...> */

#endif /* NUCCODEC_O0 */

#ifdef NUCCODEC_O1

#include "../tvclib/bbuf.h"
#include "../tvclib/cbufint64.h"
#include "../tvclib/cbufstr.h"
#include <stdbool.h>

#define NUCCODEC_WINDOW_SZ 10

#endif /* NUCCODEC_O1 */

/*
 * Encoder
 * -----------------------------------------------------------------------------
 */

#ifdef NUCCODEC_O0

typedef struct nucenc_t_ {
    size_t blkl_n;   /* no. of records processed in the curr block */
    str_t* residues; /* prediction residues (passed to the entropy coder) */
} nucenc_t;

#endif /* NUCCODEC_O0 */

#ifdef NUCCODEC_O1

typedef struct nucenc_t_ {
    size_t blkl_n; /* no. of records processed in the curr block        */
    bool first;    /* 'false', if first line has not been processed yet */

    cbufint64_t* pos_cbuf;    /* circular buffer for POSitions          */
    cbufstr_t*   cigar_cbuf;  /* circular buffer for CIGARs             */
    cbufstr_t*   exp_cbuf;    /* circular buffer for EXPanded sequences */

    bbuf_t*      pos_residues;     /* position prediction residues */
    str_t*       cigar_residues;   /* CIGAR prediction residues    */
    str_t*       seq_residues;     /* sequence prediction residues */
} nucenc_t;

#endif /* NUCCODEC_O1 */

nucenc_t* nucenc_new(void);
void nucenc_free(nucenc_t* nucenc);
void nucenc_add_record(nucenc_t*      nucenc,
                       const uint32_t  pos,
                       const char*    cigar,
                       const char*    seq);
size_t nucenc_write_block(nucenc_t* nucenc, FILE* ofp);

/*
 * Decoder
 * -----------------------------------------------------------------------------
 */

#ifdef NUCCODEC_O0

typedef struct nucdec_t_ {

} nucdec_t;

#endif /* NUCCODEC_O0 */

#ifdef NUCCODEC_O1

typedef struct nucdec_t_ {

} nucdec_t;

#endif /* NUCCODEC_O1 */

nucdec_t* nucdec_new(void);
void nucdec_free(nucdec_t* nucdec);
void nucdec_decode_block(nucdec_t* nucdec,
                         FILE*     ifp,
                         uint32_t*  pos,
                         str_t**   cigar,
                         str_t**   seq);

#endif /* TSC_NUCCODEC_H */

