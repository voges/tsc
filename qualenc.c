/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "qualenc.h"
#include "tsclib.h"

static void qualenc_init(qualenc_t* qualenc, unsigned int block_sz)
{
    qualenc->block_sz = block_sz;
}

qualenc_t* qualenc_new(unsigned int block_sz, unsigned int window_sz)
{
    qualenc_t* qualenc = (qualenc_t*)tsc_malloc_or_die(sizeof(qualenc_t));
    qualenc->buf = circstrbuf_new(window_sz);
    qualenc_init(qualenc, block_sz);
    return qualenc;
}

void qualenc_free(qualenc_t* qualenc)
{
    if (qualenc != NULL) {
        circstrbuf_free(qualenc->buf);
        free((void*)qualenc);
        qualenc = NULL;
    } else { /* fileenc == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

void qualenc_add_record(qualenc_t* qualenc, const char* qual)
{
    circstrbuf_add(qualenc->buf, qual);
    DEBUG("Added qual to buffer");
}

void qualenc_output_records(qualenc_t* qualenc, FILE* fp)
{

}

