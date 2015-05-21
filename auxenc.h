/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_AUXENC_H
#define TSC_AUXENC_H

#include "str.h"
#include <stdio.h>

typedef struct auxenc_t_ {
    unsigned int block_sz;
    str_t** aux_buf;
    unsigned int buf_pos;
} auxenc_t;

auxenc_t* auxenc_new(unsigned int block_sz);
void auxenc_free(auxenc_t* auxenc);
void auxenc_add_record(auxenc_t* auxenc, const char* aux);
void auxenc_output_records(auxenc_t* auxenc, FILE* fp);

#endif // TSC_AUXENC_H

