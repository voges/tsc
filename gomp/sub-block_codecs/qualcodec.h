/*
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>
 *
 * This file is part of tsc.
 */

#ifndef TSC_QUALCODEC_H
#define TSC_QUALCODEC_H

#include <stdio.h>
#include <stdlib.h>
#include "../str/str.h"

/*
 * Encoder
 * -----------------------------------------------------------------------------
 */

typedef struct qualenc_t_ {
    size_t     blkl_n;    /* no. of records processed in the curr block */
    str_t*     residues;  /* prediction residues                        */
} qualenc_t;

qualenc_t* qualenc_new(void);
void qualenc_free(qualenc_t* qualenc);
void qualenc_add_record(qualenc_t* qualenc, const char* qual);
size_t qualenc_write_block(qualenc_t* qualenc, FILE* ofp);

/*
 * Decoder
 * -----------------------------------------------------------------------------
 */

typedef struct qualdec_t_ {

} qualdec_t;

qualdec_t* qualdec_new(void);
void qualdec_free(qualdec_t* qualdec);
void qualdec_decode_block(qualdec_t* qualdec, FILE* ifp, str_t** qual);

#endif /* TSC_QUALCODEC_H */

