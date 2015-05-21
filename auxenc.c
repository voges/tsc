/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "auxenc.h"
#include "tsclib.h"

static void auxenc_init(auxenc_t* auxenc)
{
    auxenc->buf_pos = 0;
}

auxenc_t* auxenc_new(unsigned int block_sz)
{
    auxenc_t* auxenc = (auxenc_t*)tsc_malloc_or_die(sizeof(auxenc_t));
    auxenc->block_sz = block_sz;
    auxenc->aux_buf = (str_t**)tsc_malloc_or_die(sizeof(str_t*) * block_sz);
    unsigned int i = 0;
    for (i = 0; i < block_sz; i++) {
        auxenc->aux_buf[i] = str_new();
    }
    auxenc_init(auxenc);
    return auxenc;
}

void auxenc_free(auxenc_t* auxenc)
{
    if (auxenc != NULL) {
        unsigned int i = 0;
        for (i = 0; i < auxenc->block_sz; i++) {
            str_free(auxenc->aux_buf[i]);
        }
        free((void*)auxenc->aux_buf);
        free((void*)auxenc);
        auxenc = NULL;
    } else { /* fileenc == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

void auxenc_add_record(auxenc_t* auxenc, const char* qual)
{

}

void auxenc_output_records(auxenc_t* auxenc, FILE* fp)
{

}

