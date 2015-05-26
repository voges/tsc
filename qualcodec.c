/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "qualcodec.h"
#include "tsclib.h"

/*****************************************************************************
 * Encoder                                                                   *
 *****************************************************************************/
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

void qualenc_write_block(qualenc_t* qualenc, fwriter_t* fwriter)
{
    DEBUG("Writing block ...");

    /* Write block header (4 bytes) */
    fwriter_write_cstr(fwriter, "QUAL");             /*< this is a quality block        */
    fwriter_write_uint64(fwriter, qualenc->block_nb); /*< total number of bytes in block */



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

