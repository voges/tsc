/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_QUALENC_H
#define TSC_QUALENC_H

#include "str.h"
#include "circstrbuf.h"
#include <stdio.h>

typedef struct qualenc_t_ {
    unsigned int block_sz;
    circstrbuf_t* buf;
    unsigned char* enc_buf;
} qualenc_t;

qualenc_t* qualenc_new(unsigned int block_sz, unsigned int window_sz);
void qualenc_free(qualenc_t* qualenc);
void qualenc_add_record(qualenc_t* qualenc, const char* qual);
void qualenc_output_records(qualenc_t* qualenc, FILE* fp);

#endif // TSC_QUALENC_H

