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
static void qualenc_init(qualenc_t* qualenc)
{
    qualenc->block_lc = 0;
}

qualenc_t* qualenc_new(void)
{
    qualenc_t* qualenc = (qualenc_t*)tsc_malloc_or_die(sizeof(qualenc_t));
    qualenc->qual_cbuf = cbufstr_new(QUALCODEC_WINDOW_SZ);
    qualenc->out_buf = str_new();
    qualenc_init(qualenc);
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
    qualenc->block_lc = 0;
    cbufstr_clear(qualenc->qual_cbuf);
    str_clear(qualenc->out_buf);
}

size_t qualenc_write_block(qualenc_t* qualenc, FILE* ofp)
{
    /* Write block:
     * - unsigned char[8]: "QUAL----"
     * - uint32_t        : no. of lines in block
     * - uint32_t        : block size
     * - unsigned char[] : data
     */
    size_t header_byte_cnt = 0;
    size_t data_byte_cnt = 0;
    size_t byte_cnt = 0;

    header_byte_cnt += fwrite_cstr(ofp, "QUAL----");
    header_byte_cnt += fwrite_uint32(ofp, qualenc->block_lc);

    /* Compress block with AC. */
    unsigned char* ac_in = (unsigned char*)qualenc->out_buf->s;
    unsigned int ac_in_sz = qualenc->out_buf->n;
    unsigned int ac_out_sz = 0;
    unsigned char* ac_out = arith_compress_O0(ac_in, ac_in_sz, &ac_out_sz);
    if (tsc_verbose) tsc_log("Compressed QUAL block with AC: %zu bytes -> %zu bytes\n", ac_in_sz, ac_out_sz);

    /* Write compressed block to ofp. */
    header_byte_cnt += fwrite_uint32(ofp, ac_out_sz);
    data_byte_cnt += fwrite_buf(ofp, ac_out, ac_out_sz);
    free((void*)ac_out);
    byte_cnt = header_byte_cnt + data_byte_cnt;

    /* Log this if in verbose mode. */
    if (tsc_verbose)
        tsc_log("\tWrote QUAL block:\n"
                "\t  lines : %12zu\n"
                "\t  header: %12zu bytes\n"
                "\t  data  : %12zu bytes\n"
                "\t  total : %12zu bytes\n",
                (size_t)qualenc->block_lc,
                header_byte_cnt,
                data_byte_cnt,
                byte_cnt);

    /* Reset encoder for next block. */
    qualenc_reset(qualenc);

    return byte_cnt;
}

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
static void qualdec_init(qualdec_t* qualdec)
{
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
    qualdec->block_lc = 0;
    cbufstr_clear(qualdec->qual_cbuf);
}

void qualdec_decode_block(qualdec_t* qualdec, FILE* ifp, str_t** qual)
{
    /* Read block:
     * - unsigned char[8]: "QUAL----"
     * - uint32_t        : no. of lines in block
     * - uint32_t        : block size
     * - unsigned char[] : data
     */
    size_t header_byte_cnt = 0;
    size_t data_byte_cnt = 0;
    size_t byte_cnt = 0;

    /* Read and check block header. */
    unsigned char block_id[8];
    uint32_t block_sz;

    if (fread_buf(ifp, block_id, sizeof(block_id)) != sizeof(block_id))
        tsc_error("Could not read block ID!\n");
    if (fread_uint32(ifp, &(qualdec->block_lc)) != sizeof(qualdec->block_lc))
        tsc_error("Could not read no. of lines in block!\n");
    if (fread_uint32(ifp, &block_sz) != sizeof(block_sz))
        tsc_error("Could not read block size!\n");
    if (strncmp("QUAL----", (const char*)block_id, sizeof(block_id)))
        tsc_error("Wrong block ID: %s\n", block_id);

    header_byte_cnt += sizeof(block_id) + sizeof(qualdec->block_lc) + sizeof(block_sz);
    data_byte_cnt += block_sz;
    byte_cnt = header_byte_cnt + data_byte_cnt;

    /* Log this if in verbose mode. */
    if (tsc_verbose)
        tsc_log("\tReading QUAL block:\n"
                "\t  lines : %12zu\n"
                "\t  header: %12zu bytes\n"
                "\t  data  : %12zu bytes\n"
                "\t  total : %12zu bytes\n",
                (size_t)qualdec->block_lc,
                header_byte_cnt,
                data_byte_cnt,
                byte_cnt);

    /* Read block. */
    bbuf_t* bbuf = bbuf_new();
    bbuf_reserve(bbuf, block_sz);
    fread_buf(ifp, bbuf->bytes, block_sz);

    /* Uncompress block with AC. */
    unsigned char* ac_in = bbuf->bytes;
    unsigned int ac_in_sz = block_sz;
    unsigned int ac_out_sz = 0;
    unsigned char* ac_out = arith_uncompress_O0(ac_in, ac_in_sz, &ac_out_sz);
    if (tsc_verbose) tsc_log("Decompressed QUAL block with AC: %zu bytes -> %zu bytes\n", ac_in_sz, ac_out_sz);
    bbuf_free(bbuf);

    /* Decode block. */
    size_t i = 0;
    size_t line = 0;
    for (i = 0; i < ac_out_sz; i++) {
        if (ac_out[i] != '\n')
            str_append_char(qual[line], (const char)ac_out[i]);
        else
            line++;
    }

    free((void*)ac_out);

    /* Reset decoder for next block. */
    qualdec_reset(qualdec);
}

