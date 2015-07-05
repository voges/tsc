/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "auxcodec.h"
#include "../arithcodec/arithcodec.h"
#include "bbuf.h"
#include "./crc64/crc64.h"
#include "./frw/frw.h"
#include "tsclib.h"
#include <inttypes.h>
#include <string.h>

/*
 * AUX block format:
 *   unsigned char  blk_id[8] : "AUX-----"
 *   uint64_t       blkl_cnt  : no. of lines in block
 *   uint64_t       data_sz   : data size
 *   uint64_t       data_crc  : CRC64 of unencoded data
 *   unsigned char* data      : data
 */

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
static void auxenc_init(auxenc_t* auxenc)
{
    auxenc->blkl_cnt = 0;
}

auxenc_t* auxenc_new(void)
{
    auxenc_t* auxenc = (auxenc_t*)tsc_malloc(sizeof(auxenc_t));
    auxenc->out = str_new();
    auxenc_init(auxenc);
    return auxenc;
}

void auxenc_free(auxenc_t* auxenc)
{
    if (auxenc != NULL) {
        str_free(auxenc->out);
        free(auxenc);
        auxenc = NULL;
    } else { /* auxenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void auxenc_add_record_o0(auxenc_t*      auxenc,
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

    str_append_cstr(auxenc->out, qname);
    str_append_cstr(auxenc->out, "\t");
    str_append_cstr(auxenc->out, flag_cstr);
    str_append_cstr(auxenc->out, "\t");
    str_append_cstr(auxenc->out, rname);
    str_append_cstr(auxenc->out, "\t");
    str_append_cstr(auxenc->out, mapq_cstr);
    str_append_cstr(auxenc->out, "\t");
    str_append_cstr(auxenc->out, rnext);
    str_append_cstr(auxenc->out, "\t");
    str_append_cstr(auxenc->out, pnext_cstr);
    str_append_cstr(auxenc->out, "\t");
    str_append_cstr(auxenc->out, tlen_cstr);
    str_append_cstr(auxenc->out, "\t");
    str_append_cstr(auxenc->out, opt);
    str_append_cstr(auxenc->out, "\n");

    auxenc->blkl_cnt++;
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
    auxenc_add_record_o0(auxenc, qname, flag, rname, mapq, rnext, pnext, tlen,
                         opt);
}

static void auxenc_reset(auxenc_t* auxenc)
{
    auxenc->blkl_cnt = 0;
    str_clear(auxenc->out);
}

size_t auxenc_write_block(auxenc_t* auxenc, FILE* ofp)
{
    size_t blk_sz = 0;

    /* Compress block with an entropy coder. */
    unsigned char* in = (unsigned char*)auxenc->out->s;
    unsigned int in_sz = (unsigned int)auxenc->out->len;
    unsigned int data_sz = 0;
    unsigned char* data = arith_compress_o0(in, in_sz, &data_sz);

    if (tsc_verbose)
        tsc_log("Compressed AUX block: %zu bytes -> %zu bytes (%6.2f%%)\n",
                in_sz, data_sz, (double)data_sz / (double)in_sz*100);

    /* Compute CRC64. */
    uint64_t data_crc = crc64(data, data_sz);

    /* Write compressed block. */
    unsigned char blk_id[8] = "AUX----"; blk_id[7] = '\0';
    blk_sz += fwrite_buf(ofp, blk_id, sizeof(blk_id));
    blk_sz += fwrite_uint64(ofp, (uint64_t)auxenc->blkl_cnt);
    blk_sz += fwrite_uint64(ofp, (uint64_t)data_sz);
    blk_sz += fwrite_uint64(ofp, (uint64_t)data_crc);
    blk_sz += fwrite_buf(ofp, data, data_sz);

    if (tsc_verbose)
        tsc_log("Wrote AUX block: %zu lines, %zu data bytes\n",
                auxenc->blkl_cnt, data_sz);

    auxenc_reset(auxenc); /* reset encoder for next block */
    free(data); /* free memory used for encoded bitstream */

    return blk_sz;
}

/*****************************************************************************
 * Decoder                                                                   *
 *****************************************************************************/
static void auxdec_init(auxdec_t* auxdec)
{

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
        free(auxdec);
        auxdec = NULL;
    } else { /* auxdec == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

void auxdec_decode_block(auxdec_t* auxdec, FILE* ifp, str_t** aux)
{
    unsigned char  blk_id[8];
    uint64_t       blkl_cnt;
    uint64_t       data_sz;
    uint64_t       data_crc;
    unsigned char* data;

    /* Read and check block header. */
    if (fread_buf(ifp, blk_id, sizeof(blk_id)) != sizeof(blk_id))
        tsc_error("Could not read block ID!\n");
    if (strncmp("AUX----", (const char*)blk_id, 7))
        tsc_error("Wrong block ID: %s\n", blk_id);
    if (fread_uint64(ifp, &blkl_cnt) != sizeof(blkl_cnt))
        tsc_error("Could not read no. of lines in block!\n");
    if (fread_uint64(ifp, &data_sz) != sizeof(data_sz))
        tsc_error("Could not read size of data section!\n");
    if (fread_uint64(ifp, &data_crc) != sizeof(data_crc))
        tsc_error("Could not read CRC for data section!\n");

    if (tsc_verbose)
        tsc_log("Reading AUX block: %zu lines, %zu data bytes\n", blkl_cnt,
                data_sz);

    /* Read and decompress block. */
    data = (unsigned char*)tsc_malloc((size_t)data_sz);
    fread_buf(ifp, data, data_sz);
    unsigned int out_sz = 0;
    unsigned char* out = arith_decompress_o0(data, data_sz, &out_sz);
    free(data);

    if (tsc_verbose)
        tsc_log("Decompressed AUX block: %zu bytes -> %zu bytes (%5.2f%%)\n",
                data_sz, out_sz, (double)out_sz/(double)data_sz*100);

    /* Decode block. */
    size_t i = 0;
    size_t line = 0;
    for (i = 0; i < out_sz; i++) {
        if (out[i] != '\n')
            str_append_char(aux[line], (const char)out[i]);
        else
            line++;
    }

    free(out); /* free memory used for decoded bitstream */
}

