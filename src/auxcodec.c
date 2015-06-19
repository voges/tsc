/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "auxcodec.h"
#include "accodec.h"
#include "bbuf.h"
#include "tsclib.h"
#include "frw.h"
#include <inttypes.h>
#include <string.h>

/*****************************************************************************
 * Encoder                                                                   *
 *****************************************************************************/
static void auxenc_init(auxenc_t* auxenc)
{
    auxenc->block_lc = 0;
}

auxenc_t* auxenc_new(void)
{
    auxenc_t* auxenc = (auxenc_t*)tsc_malloc(sizeof(auxenc_t));
    auxenc->out_buf = str_new();
    auxenc_init(auxenc);
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

void auxenc_add_record(auxenc_t*      auxenc,
                       const char*    qname,
                       const uint64_t flag,
                       const char*    rname,
                       const uint64_t mapq,
                       const char*    rnext,
                       const uint64_t pnext,
                       const uint64_t tlen,
                       const char*    opt)
{
    char flag_cstr[101];  /* this should be enough space */
    char mapq_cstr[101];  /* this should be enough space */
    char pnext_cstr[101]; /* this should be enough space */
    char tlen_cstr[101];  /* this should be enough space */
    snprintf(flag_cstr, sizeof(flag_cstr), "%"PRIu64, flag);
    snprintf(mapq_cstr, sizeof(mapq_cstr), "%"PRIu64, mapq);
    snprintf(pnext_cstr, sizeof(pnext_cstr), "%"PRIu64, pnext);
    snprintf(tlen_cstr, sizeof(tlen_cstr), "%"PRIu64, tlen);

    str_append_cstr(auxenc->out_buf, qname);
    str_append_cstr(auxenc->out_buf, "\t");
    str_append_cstr(auxenc->out_buf, flag_cstr);
    str_append_cstr(auxenc->out_buf, "\t");
    str_append_cstr(auxenc->out_buf, rname);
    str_append_cstr(auxenc->out_buf, "\t");
    str_append_cstr(auxenc->out_buf, mapq_cstr);
    str_append_cstr(auxenc->out_buf, "\t");
    str_append_cstr(auxenc->out_buf, rnext);
    str_append_cstr(auxenc->out_buf, "\t");
    str_append_cstr(auxenc->out_buf, pnext_cstr);
    str_append_cstr(auxenc->out_buf, "\t");
    str_append_cstr(auxenc->out_buf, tlen_cstr);
    str_append_cstr(auxenc->out_buf, "\t");
    str_append_cstr(auxenc->out_buf, opt);
    str_append_cstr(auxenc->out_buf, "\n");

    auxenc->block_lc++;
}

static void auxenc_reset(auxenc_t* auxenc)
{
    auxenc->block_lc = 0;
    str_clear(auxenc->out_buf);
}

size_t auxenc_write_block(auxenc_t* auxenc, FILE* ofp)
{
    /* Write block:
     * - unsigned char[8]: "AUX-----"
     * - uint32_t        : no. of lines in block
     * - uint32_t        : block size
     * - unsigned char[] : data
     */
    size_t header_byte_cnt = 0;
    size_t data_byte_cnt = 0;
    size_t byte_cnt = 0;

    header_byte_cnt += fwrite_cstr(ofp, "AUX-----");
    header_byte_cnt += fwrite_uint32(ofp, auxenc->block_lc);

    /* Compress block with AC. */
    unsigned char* ac_in = (unsigned char*)auxenc->out_buf->s;
    unsigned int ac_in_sz = auxenc->out_buf->n;
    unsigned int ac_out_sz = 0;
    unsigned char* ac_out = arith_compress_O0(ac_in, ac_in_sz, &ac_out_sz);
    if (tsc_verbose) tsc_log("Compressed AUX block with AC: %zu bytes -> %zu bytes\n", ac_in_sz, ac_out_sz);

    /* Write compressed block to ofp. */
    header_byte_cnt += fwrite_uint32(ofp, ac_out_sz);
    data_byte_cnt += fwrite_buf(ofp, ac_out, ac_out_sz);
    free((void*)ac_out);
    byte_cnt = header_byte_cnt + data_byte_cnt;

    /* Log this if in verbose mode. */
    if (tsc_verbose)
        tsc_log("\tWrote AUX block:\n"
                "\t  lines : %12zu\n"
                "\t  header: %12zu bytes\n"
                "\t  data  : %12zu bytes\n"
                "\t  total : %12zu bytes\n",
                (size_t)auxenc->block_lc,
                header_byte_cnt,
                data_byte_cnt,
                byte_cnt);

    /* Reset encoder for next block. */
    auxenc_reset(auxenc);

    return byte_cnt;
}

/*****************************************************************************
 * Decoder                                                                   *
 *****************************************************************************/
static void auxdec_init(auxdec_t* auxdec)
{
    auxdec->block_lc = 0;
}

auxdec_t* auxdec_new(void)
{
    auxdec_t* auxdec = (auxdec_t*)tsc_malloc(sizeof(auxdec_t));
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
    auxdec->block_lc = 0;
}

void auxdec_decode_block(auxdec_t* auxdec, FILE* ifp, str_t** aux)
{
    /* Read block:
     * - unsigned char[8]: "AUX-----"
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
    if (fread_uint32(ifp, &(auxdec->block_lc)) != sizeof(auxdec->block_lc))
        tsc_error("Could not read no. of lines in block!\n");
    if (fread_uint32(ifp, &block_sz) != sizeof(block_sz))
        tsc_error("Could not read block size!\n");
    if (strncmp("AUX-----", (const char*)block_id, sizeof(block_id)))
        tsc_error("Wrong block ID: %s\n", block_id);

    header_byte_cnt += sizeof(block_id) + sizeof(auxdec->block_lc) + sizeof(block_sz);
    data_byte_cnt += block_sz;
    byte_cnt = header_byte_cnt + data_byte_cnt;

    /* Log this if in verbose mode. */
    if (tsc_verbose)
        tsc_log("\tReading AUX block:\n"
                "\t  lines : %12zu\n"
                "\t  header: %12zu bytes\n"
                "\t  data  : %12zu bytes\n"
                "\t  total : %12zu bytes\n",
                (size_t)auxdec->block_lc,
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
    if (tsc_verbose) tsc_log("Uncompressed AUX block with AC: %zu bytes -> %zu bytes\n", ac_in_sz, ac_out_sz);
    bbuf_free(bbuf);

    /* Decode block. */
    size_t i = 0;
    size_t line = 0;
    for (i = 0; i < ac_out_sz; i++) {
        if (ac_out[i] != '\n')
            str_append_char(aux[line], (const char)ac_out[i]);
        else
            line++;
    }

    free((void*)ac_out);

    /* Reset decoder for next block. */
    auxdec_reset(auxdec);
}

