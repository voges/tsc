/*
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>
 *
 * This file is part of tsc.
 */

#ifndef TSC_QUALCODEC_H
#define TSC_QUALCODEC_H

#define QUALCODEC_O0
//#define QUALCODEC_O1
//#define QUALCODEC_O2

#include <stdio.h>
#include <stdlib.h>

#ifdef QUALCODEC_O0

#include "../str/str.h"

#endif /* QUALCODEC_O0 */

#ifdef QUALCODEC_O1

#include "cbufint64.h"
#include "cbufstr.h"
#define QUALCODEC_WINDOW_SZ 100

#endif /* QUALCODEC_O1 */

#ifdef QUALCODEC_O1

#include "cbufint64.h"
#include "cbufstr.h"
#define QUALCODEC_WINDOW_SZ 100

#endif /* QUALCODEC_O1 */

/*
 * Encoder
 * -----------------------------------------------------------------------------
 */

#ifdef QUALCODEC_O0

typedef struct qualenc_t_ {
    size_t     blkl_n;    /* no. of records processed in the curr block */
    str_t*     residues;  /* prediction residues                        */
} qualenc_t;

#endif /* QUALCODEC_O0 */

#ifdef QUALCODEC_O1

typedef struct qualenc_t_ {
    size_t     blkl_n;    /* no. of records processed in the curr block */
    cbufstr_t* qual_cbuf; /* circular buffer for QUALity scores         */
    str_t*     residues;  /* prediction residues                        */
} qualenc_t;

#endif /* QUALCODEC_O1 */

#ifdef QUALCODEC_O2

#define QUALCODEC_MAX 126
#define QUALCODEC_MIN 33
#define QUALCODEC_RANGE (QUALCODEC_MAX-QUALCODEC_MIN)
#define QUALCODEC_TABLE_SZ (QUALCODEC_RANGE*QUALCODEC_RANGE*QUALCODEC_RANGE)
#define QUALCODEC_MEM_SZ 3

typedef struct qualenc_t_ {
    size_t     blkl_n;    /* no. of records processed in the curr block */
    cbufstr_t* qual_cbuf; /* circular buffer for QUALity scores         */
    str_t*     residues;  /* prediction residues                        */

    cbufint64_t* qual_cbuf_len;
    cbufint64_t* qual_cbuf_mu;
    cbufint64_t* qual_cbuf_var;
    unsigned int freq[QUALCODEC_TABLE_SZ][QUALCODEC_RANGE]; /* 300 MiB */
    char         pred[QUALCODEC_TABLE_SZ];
} qualenc_t;

#endif /* QUALCODEC_O2 */

qualenc_t* qualenc_new(void);
void qualenc_free(qualenc_t* qualenc);
void qualenc_add_record(qualenc_t* qualenc, const char* qual);
size_t qualenc_write_block(qualenc_t* qualenc, FILE* ofp);

/*
 * Decoder
 * -----------------------------------------------------------------------------
 */

#ifdef QUALCODEC_O0

typedef struct qualdec_t_ {

} qualdec_t;

#endif /* QUALCODEC_O0 */

#ifdef QUALCODEC_O1

typedef struct qualdec_t_ {

} qualdec_t;

#endif /* QUALCODEC_O1 */

#ifdef QUALCODEC_O2

typedef struct qualdec_t_ {

} qualdec_t;

#endif /* QUALCODEC_O2 */

qualdec_t* qualdec_new(void);
void qualdec_free(qualdec_t* qualdec);
void qualdec_decode_block(qualdec_t* qualdec, FILE* ifp, str_t** qual);

#endif /* TSC_QUALCODEC_H */

