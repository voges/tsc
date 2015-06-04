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
    auxenc->block_lc = 0;
}

auxenc_t* auxenc_new(const size_t block_sz)
{
    auxenc_t* auxenc = (auxenc_t*)tsc_malloc_or_die(sizeof(auxenc_t));
    auxenc->out_buf = str_new();
    auxenc_init(auxenc, block_sz);
    return auxenc;
}

void auxenc_free(auxenc_t* auxenc)
{
    if (auxenc != NULL) {
        str_free(auxenc->out_buf);
        free((void*)auxenc);
        auxenc = NULL;
    } else { /* auxenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
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
    /* TODO: Add int fields to aux_buf */
    str_append_cstr(auxenc->out_buf, qname);
    str_append_cstr(auxenc->out_buf, "\t");
    str_append_cstr(auxenc->out_buf, rname);
    str_append_cstr(auxenc->out_buf, "\t");
    str_append_cstr(auxenc->out_buf, rnext);
    str_append_cstr(auxenc->out_buf, "\t");
    str_append_cstr(auxenc->out_buf, opt);
    str_append_cstr(auxenc->out_buf, "\n");
    auxenc->block_lc++;
}

static void auxenc_reset(auxenc_t* auxenc)
{
    auxenc->block_sz = 0;
    auxenc->block_lc = 0;
    str_clear(auxenc->out_buf);
}

void auxenc_write_block(auxenc_t* auxenc, FILE* ofp)
{
    tsc_log("Writing AUX- block: %zu bytes\n", auxenc->out_buf->n);

    /*
     * Write block:
     * - 4 bytes: identifier
     * - 8 bytes: number of bytes (n)
     * - n bytes: block content
     */
    fwrite_cstr(ofp, "AUX-");
    fwrite_uint64(ofp, (uint64_t)auxenc->out_buf->n);
    fwrite_buf(ofp, (unsigned char*)auxenc->out_buf->s, auxenc->out_buf->n);

    /* Reset encoder for next block */
    auxenc_reset(auxenc);
}

/*****************************************************************************
 * Decoder                                                                   *
 *****************************************************************************/
static void auxdec_init(auxdec_t* auxdec)
{
    auxdec->block_sz = 0;
    auxdec->block_lc = 0;
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
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void auxdec_reset(auxdec_t* auxdec)
{
    auxdec->block_sz = 0;
    auxdec->block_lc = 0;
}

void auxdec_decode_block(auxdec_t* auxdec, FILE* ifp, str_t** seq)
{
    /*
     * Read block header:
     * - 4 bytes: identifier (must equal "AUX-")
     * - 8 bytes: number of bytes in block
     */
    unsigned char block_id[4];
    fread_buf(ifp, block_id, 4);
    uint64_t block_nb;
    fread_uint64(ifp, &block_nb);
    tsc_log("Reading AUX- block: %zu bytes\n", block_nb);

    /* Decode block content with block_nb bytes */
    /* TODO */

    /* Reset decoder for next block */
    auxdec_reset(auxdec);
}

