/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "qualcodec.h"
#include "tsclib.h"
#include "frw.h"

/*****************************************************************************
 * Encoder                                                                   *
 *****************************************************************************/
static void qualenc_init(qualenc_t* qualenc, const size_t block_sz, const size_t window_sz)
{
    qualenc->block_sz = block_sz;
    qualenc->window_sz = window_sz;
}

qualenc_t* qualenc_new(const size_t block_sz)
{
    qualenc_t* qualenc = (qualenc_t*)tsc_malloc_or_die(sizeof(qualenc_t));
    qualenc->qual_buf = cbufstr_new(QUALCODEC_WINDOW_SZ);
    qualenc_init(qualenc, block_sz, QUALCODEC_WINDOW_SZ);
    return qualenc;
}

void qualenc_free(qualenc_t* qualenc)
{
    if (qualenc != NULL) {
        cbufstr_free(qualenc->qual_buf);
        free((void*)qualenc);
        qualenc = NULL;
    } else { /* fileenc == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.\n");
    }
}

void qualenc_add_record(qualenc_t* qualenc, const char* qual)
{
    cbufstr_push(qualenc->qual_buf, qual);
    DEBUG("Added qual to buffer");
}

void qualenc_write_block(qualenc_t* qualenc, FILE* fp)
{
    DEBUG("Writing block ...");

    /* Write block header (4 bytes) */
    fwrite_cstr(fp, "QUAL");             /*< this is a quality block        */
    fwrite_uint64(fp, qualenc->block_b); /*< total number of bytes in block */



    //qualenc_reset();
}

/*****************************************************************************
 * Decoder                                                                   *
 *****************************************************************************/
static void qualdec_init(qualdec_t* qualdec)
{

}

qualdec_t* qualdec_new(void)
{
    qualdec_t* qualdec = (qualdec_t*)tsc_malloc_or_die(sizeof(qualdec_t));

    qualdec_init(qualdec);
    return qualdec;
}

void qualdec_free(qualdec_t* qualdec)
{

}

void qualdec_decode_block(qualdec_t* qualdec, FILE* fp)
{

}

