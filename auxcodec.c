/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "auxcodec.h"
#include "tsclib.h"

/*****************************************************************************
 * Encoder                                                                   *
 *****************************************************************************/
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

void auxenc_add_record(auxenc_t* auxenc,
                       const char* qname,
                       uint64_t flag,
                       const char* rname,
                       uint64_t mapq,
                       const char* rnext,
                       uint64_t pnext,
                       uint64_t tlen,
                       const char* opt)
{

}

void auxenc_write_block(auxenc_t* auxenc, fwriter_t* fwriter)
{

}

/*****************************************************************************
 * Decoder                                                                   *
 *****************************************************************************/
static void auxdec_init(auxdec_t* auxdec)
{

}

auxdec_t* auxdec_new(void)
{
    auxdec_t* auxdec = (auxdec_t*)tsc_malloc_or_die(sizeof(auxdec_t));

    auxdec_init(auxdec);
    return auxdec;
}

void auxdec_free(auxdec_t* auxdec)
{
    if (auxdec != NULL) {
        free((void*)auxdec);
        auxdec = NULL;
    } else { /* auxdec == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

