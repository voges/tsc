/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "qualcodec.h"
#include "frw.h"
#include "tsclib.h"

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
static void qualenc_init(qualenc_t* qualenc, const size_t block_sz)
{
    qualenc->block_sz = block_sz;
    qualenc->block_lc = 0;
}

qualenc_t* qualenc_new(const size_t block_sz)
{
    qualenc_t* qualenc = (qualenc_t*)tsc_malloc_or_die(sizeof(qualenc_t));
    qualenc->qual_cbuf = cbufstr_new(QUALCODEC_WINDOW_SZ);
    qualenc->out_buf = str_new();
    qualenc_init(qualenc, block_sz);
    return qualenc;
}

void qualenc_free(qualenc_t* qualenc)
{
    if (qualenc != NULL) {
        cbufstr_free(qualenc->qual_cbuf);
        str_free(qualenc->out_buf);
        free((void*)qualenc);
        qualenc = NULL;
    } else { /* qualenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

void qualenc_add_record(qualenc_t* qualenc, const char* qual)
{
    //cbufstr_push(qualenc->qual_cbuf, qual);
    str_append_cstr(qualenc->out_buf, qual);
    str_append_cstr(qualenc->out_buf, "\n");
}

static void qualenc_reset(qualenc_t* qualenc)
{
    qualenc->block_sz = 0;
    qualenc->block_lc = 0;
    cbufstr_clear(qualenc->qual_cbuf);
    str_clear(qualenc->out_buf);
}

size_t qualenc_write_block(qualenc_t* qualenc, FILE* ofp)
{
    tsc_log("Writing QUAL block: %zu bytes\n", qualenc->out_buf->n);

    /*
     * Write block:
     * - 4 bytes: identifier
     * - 8 bytes: number of bytes (n)
     * - n bytes: block content
     */
    size_t nbytes = 0;
    nbytes += fwrite_cstr(ofp, "QUAL");
    nbytes += fwrite_uint64(ofp, (uint64_t)qualenc->out_buf->n);
    nbytes += fwrite_buf(ofp, (unsigned char*)qualenc->out_buf->s, qualenc->out_buf->n);

    /* Reset encoder for next block */
    qualenc_reset(qualenc);
    
    return nbytes;
}

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
static void qualdec_init(qualdec_t* qualdec)
{
    qualdec->block_sz = 0;
    qualdec->block_lc = 0;
}

qualdec_t* qualdec_new(void)
{
    qualdec_t* qualdec = (qualdec_t*)tsc_malloc_or_die(sizeof(qualdec_t));
    qualdec->qual_cbuf = cbufstr_new(QUALCODEC_WINDOW_SZ);
    qualdec_init(qualdec);
    return qualdec;
}

void qualdec_free(qualdec_t* qualdec)
{
    if (qualdec != NULL) {
        cbufstr_free(qualdec->qual_cbuf);
        free((void*)qualdec);
        qualdec = NULL;
    } else { /* qualdec == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void qualdec_reset(qualdec_t* qualdec)
{
    qualdec->block_sz = 0;
    qualdec->block_lc = 0;
    cbufstr_clear(qualdec->qual_cbuf);
}

void qualdec_decode_block(qualdec_t* qualdec, FILE* ifp, str_t** qual)
{
    /*
     * Read block header:
     * - 4 bytes: identifier (must equal "QUAL")
     * - 8 bytes: number of bytes in block
     */
    unsigned char block_id[4];
    fread_buf(ifp, block_id, 4);
    uint64_t block_nb;
    fread_uint64(ifp, &block_nb);
    tsc_log("Reading QUAL block: %zu bytes\n", block_nb);

    /* Decode block content with block_nb bytes */
    /* TODO */

    /* Reset decoder for next block */
    qualdec_reset(qualdec);
}

