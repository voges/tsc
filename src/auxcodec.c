/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "auxcodec.h"
#include "tsclib.h"
#include "frw.h"

/*****************************************************************************
 * Encoder                                                                   *
 *****************************************************************************/
static void auxenc_init(auxenc_t* auxenc, const size_t block_sz)
{
    auxenc->block_sz = block_sz;
    auxenc->block_b = 0;
    auxenc->buf_pos = 0;
}

auxenc_t* auxenc_new(const size_t block_sz)
{
    auxenc_t* auxenc = (auxenc_t*)tsc_malloc_or_die(sizeof(auxenc_t));
    auxenc->strbuf = (str_t**)tsc_malloc_or_die(sizeof(str_t*) * block_sz);
    auxenc->intbuf = (uint64_t*)tsc_malloc_or_die(sizeof(uint64_t) * block_sz);
    unsigned int i = 0;
    for (i = 0; i < block_sz; i++) {
        auxenc->strbuf[i] = str_new();
        auxenc->intbuf[i] = 0;
    }
    auxenc_init(auxenc, block_sz);
    return auxenc;
}

void auxenc_free(auxenc_t* auxenc)
{
    if (auxenc != NULL) {
        unsigned int i = 0;
        for (i = 0; i < auxenc->block_sz; i++) str_free(auxenc->strbuf[i]);
        free((void*)auxenc->intbuf);
        free((void*)auxenc);
        auxenc = NULL;
    } else { /* fileenc == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.\n");
    }
}

void auxenc_add_record(auxenc_t*   auxenc,
                       const char* qname,
                       uint64_t    flag,
                       const char* rname,
                       uint64_t    mapq,
                       const char* rnext,
                       uint64_t    pnext,
                       uint64_t    tlen,
                       const char* opt)
{
    auxenc->buf_pos++;
}

void auxenc_write_block(auxenc_t* auxenc, FILE* fp)
{
    tsc_warning("Discarding aux data!\n");
    auxenc->buf_pos = 0;
}

/*****************************************************************************
 * Decoder                                                                   *
 *****************************************************************************/
static void auxdec_init(auxdec_t* auxdec)
{
    auxdec->block_sz = 0;
    auxdec->block_b = 0;
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
        tsc_error("Tried to free NULL pointer. Aborting.\n");
    }
}

void auxdec_decode_block(auxdec_t* auxdec, FILE* fp)
{
    tsc_warning("Discarding aux block!\n");
}

