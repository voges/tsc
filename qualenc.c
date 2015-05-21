/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "qualenc.h"
#include "tsclib.h"

static void qualenc_init(qualenc_t* qualenc, const unsigned int block_sz, const unsigned int window_sz)
{
    qualenc->block_sz = block_sz;
    qualenc->window_sz = window_sz;
}

qualenc_t* qualenc_new(const unsigned int block_sz, const unsigned int window_sz)
{
    qualenc_t* qualenc = (qualenc_t*)tsc_malloc_or_die(sizeof(qualenc_t));
    qualenc->qual_buf = cbufstr_new(window_sz);
    qualenc_init(qualenc, block_sz, window_sz);
    return qualenc;
}

void qualenc_free(qualenc_t* qualenc)
{
    if (qualenc != NULL) {
        cbufstr_free(qualenc->qual_buf);
        free((void*)qualenc);
        qualenc = NULL;
    } else { /* fileenc == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

void qualenc_add_record(qualenc_t* qualenc, const char* qual)
{
    cbufstr_add(qualenc->qual_buf, qual);
    DEBUG("Added qual to buffer");
}

void qualenc_output_records(qualenc_t* qualenc, FILE* fp)
{

}

