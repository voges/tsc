/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "qualenc.h"
#include "tsclib.h"

static void qualenc_init(qualenc_t* qualenc)
{
    qualenc->buf_pos = 0;
}

qualenc_t* qualenc_new(unsigned int block_sz)
{
    qualenc_t* qualenc = (qualenc_t*)tsc_malloc_or_die(sizeof(qualenc_t));
    qualenc->block_sz = block_sz;
    qualenc->qual_buf = (str_t**)tsc_malloc_or_die(sizeof(str_t*) * block_sz);
    unsigned int i = 0;
    for (i = 0; i < block_sz; i++) {
        qualenc->qual_buf[i] = str_new();
    }
    qualenc_init(qualenc);
    return qualenc;
}

void qualenc_free(qualenc_t* qualenc)
{
    if (qualenc != NULL) {
        unsigned int i = 0;
        for (i = 0; i < qualenc->block_sz; i++) {
            str_free(qualenc->qual_buf[i]);
        }
        free((void*)qualenc->qual_buf);
        free((void*)qualenc);
        qualenc = NULL;
    } else { /* fileenc == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

void qualenc_add_record(qualenc_t* qualenc, const char* qual)
{

}

void qualenc_output_records(qualenc_t* qualenc, FILE* fp)
{

}

