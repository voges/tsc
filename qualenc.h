/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_QUALENC_H
#define TSC_QUALENC_H

#include "cbufstr.h"
#include <stdio.h>

typedef struct qualenc_t_ {
    unsigned int block_sz;
    unsigned int window_sz;
    cbufstr_t* qual_buf;
    unsigned char* out_buf;
} qualenc_t;

qualenc_t* qualenc_new(const unsigned int block_sz, const unsigned int window_sz);
void qualenc_free(qualenc_t* qualenc);
void qualenc_add_record(qualenc_t* qualenc, const char* qual);
void qualenc_output_records(qualenc_t* qualenc, FILE* fp);

#endif // TSC_QUALENC_H

