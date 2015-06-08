/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "qualcodec.h"
#include "accodec.h"
#include "bbuf.h"
#include "frw.h"
#include "tsclib.h"
#include <string.h>

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
    /*cbufstr_push(qualenc->qual_cbuf, qual);*/
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
    /*
     * Write block:
     * - 4 bytes: identifier
     * - 4 bytes: number of bytes (n)
     * - n bytes: block content
     */
    size_t nbytes = 0;
    nbytes += fwrite_cstr(ofp, "QUAL");
    //nbytes += fwrite_uint32(ofp, (uint32_t)qualenc->out_buf->n);
    //nbytes += fwrite_buf(ofp, (unsigned char*)qualenc->out_buf->s, qualenc->out_buf->n);

    tsc_log("Compressing QUAL block (%zu bytes) with arithmetic coder\n", qualenc->out_buf->n);

    unsigned char* ac_in = (unsigned char*)qualenc->out_buf->s;
    unsigned int ac_in_sz = qualenc->out_buf->n;
    unsigned int ac_out_sz = 0;
    unsigned char* ac_out = arith_compress_O0(ac_in, ac_in_sz, &ac_out_sz);

    nbytes += fwrite_uint32(ofp, ac_out_sz);
    nbytes += fwrite_buf(ofp, ac_out, ac_out_sz);
    free((void*)ac_out);

    tsc_log("Wrote QUAL block: %zu bytes\n", nbytes);

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
     * - 4 bytes: number of bytes in block
     */
    unsigned char block_id[4];
    uint32_t block_sz;

    if (fread_buf(ifp, block_id, 4) != sizeof(block_id))
        tsc_error("Could not read block header!\n");
    if (fread_uint32(ifp, &block_sz) != sizeof(block_sz))
        tsc_error("Could not read number of bytes in block!\n");
    if (strncmp("QUAL", (const char*)block_id, 4))
        tsc_error("Wrong block id: %s\n", block_id);
    tsc_log("Reading %s block: %zu bytes\n", block_id, block_sz);

    /* Decode block content with block_nb bytes */
    /* TODO */
    bbuf_t* buf = bbuf_new();
    bbuf_reserve(buf, block_sz);
    fread_buf(ifp, buf->buf, block_sz);

    unsigned char* ac_in = (unsigned char*)buf->buf;
    unsigned int ac_in_sz = block_sz;
    unsigned int ac_out_sz = 0;
    unsigned char* ac_out = arith_uncompress_O0(ac_in, ac_in_sz, &ac_out_sz);

    DEBUG("ac_out_sz: %d", ac_out_sz);
    str_clear(qual[0]);
    str_append_cstrn(qual[0], ac_out, ac_out_sz);
    DEBUG("%s", qual[0]->s);

    free((void*)ac_out);

    bbuf_free(buf);

    /* Reset decoder for next block */
    qualdec_reset(qualdec);
}

