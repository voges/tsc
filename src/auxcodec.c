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
 * Aux block format:
 *   unsigned char  blk_id[8] : "aux----" + '\0'
 *   uint64_t       blkl_n    : no. of lines in block
 *   uint64_t       data_sz   : data size
 *   uint64_t       data_crc  : CRC64 of compressed data
 *   unsigned char* data      : data
 */

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
static void auxenc_init(auxenc_t* auxenc)
{
    auxenc->blkl_n = 0;
}

auxenc_t* auxenc_new(void)
{
    auxenc_t* auxenc = (auxenc_t*)tsc_malloc(sizeof(auxenc_t));
    auxenc->residues = str_new();
    auxenc_init(auxenc);
    return auxenc;
}

void auxenc_free(auxenc_t* auxenc)
{
    if (auxenc != NULL) {
        str_free(auxenc->residues);
        free(auxenc);
        auxenc = NULL;
    } else { /* auxenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

/*
 * auxenc_add_record_o0: Fall-trough to entropy coder
 */
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

    str_append_cstr(auxenc->residues, qname);
    str_append_cstr(auxenc->residues, "\t");
    str_append_cstr(auxenc->residues, flag_cstr);
    str_append_cstr(auxenc->residues, "\t");
    str_append_cstr(auxenc->residues, rname);
    str_append_cstr(auxenc->residues, "\t");
    str_append_cstr(auxenc->residues, mapq_cstr);
    str_append_cstr(auxenc->residues, "\t");
    str_append_cstr(auxenc->residues, rnext);
    str_append_cstr(auxenc->residues, "\t");
    str_append_cstr(auxenc->residues, pnext_cstr);
    str_append_cstr(auxenc->residues, "\t");
    str_append_cstr(auxenc->residues, tlen_cstr);
    str_append_cstr(auxenc->residues, "\t");
    str_append_cstr(auxenc->residues, opt);
    str_append_cstr(auxenc->residues, "\n");

    auxenc->blkl_n++;
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
    auxenc->blkl_n = 0;
    str_clear(auxenc->residues);
}

size_t auxenc_write_block(auxenc_t* auxenc, FILE* ofp)
{
    size_t blk_sz = 0;

    /* Compress whole block with an entropy coder */
    unsigned char* residues = (unsigned char*)auxenc->residues->s;
    unsigned int residues_sz = (unsigned int)auxenc->residues->len;
    unsigned int data_sz = 0;
    unsigned char* data = arith_compress_o0(residues, residues_sz, &data_sz);

    tsc_vlog("Compressed aux block: %zu bytes -> %zu bytes (%6.2f%%)\n",
             residues_sz, data_sz, (double)data_sz / (double)residues_sz*100);

    /* Compute CRC64 */
    uint64_t data_crc = crc64(data, data_sz);

    /* Write compressed block */
    unsigned char blk_id[8] = "aux----"; blk_id[7] = '\0';
    blk_sz += fwrite_buf(ofp, blk_id, sizeof(blk_id));
    blk_sz += fwrite_uint64(ofp, (uint64_t)auxenc->blkl_n);
    blk_sz += fwrite_uint64(ofp, (uint64_t)data_sz);
    blk_sz += fwrite_uint64(ofp, (uint64_t)data_crc);
    blk_sz += fwrite_buf(ofp, data, data_sz);

    tsc_vlog("Wrote aux block: %zu lines, %zu data bytes\n",
             auxenc->blkl_n, data_sz);

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
static void auxdec_decode_block_o0(auxdec_t*      auxdec,
                                   unsigned char* residues,
                                   size_t         residues_sz,
                                   str_t**        qname,
                                   uint64_t*      flag,
                                   str_t**        rname,
                                   uint64_t*      mapq,
                                   str_t**        rnext,
                                   uint64_t*      pnext,
                                   uint64_t*      tlen,
                                   str_t**        opt)
{
    size_t i = 0;
    size_t line = 0;
    unsigned char* cstr = residues;
    unsigned int idx = 0;

    for (i = 0; i < residues_sz; i++) {
        if (residues[i] == '\n') {
            residues[i] = '\0';
            str_append_cstr(opt[line], (const char*)cstr);
            cstr = &residues[i+1];
            idx = 0;
            line++;
            continue;
        }

        if (residues[i] == '\t') {
            residues[i] = '\0';
            switch (idx++) {
            case 0:
                str_append_cstr(qname[line], (const char*)cstr);
                break;
            case 1:
                flag[line] = strtoull((const char*)cstr, NULL, 10);
                break;
            case 2:
                str_append_cstr(rname[line], (const char*)cstr);
                break;
            case 3:
                mapq[line] = strtoull((const char*)cstr, NULL, 10);
                break;
            case 4:
                str_append_cstr(rnext[line], (const char*)cstr);
                break;
            case 5:
                pnext[line] = strtoull((const char*)cstr, NULL, 10);
                break;
            case 6:
                tlen[line] = strtoull((const char*)cstr, NULL, 10);
                break;
            case 7:
                /* fall through (all upcoming columns are 'opt') */
            default:
                str_append_cstr(opt[line], (const char*)cstr);
                str_append_char(opt[line], '\t');
                break;
            }
            cstr = &residues[i+1];
        }
    }
}

void auxdec_decode_block(auxdec_t* auxdec,
                         FILE*     ifp,
                         str_t**   qname,
                         uint64_t* flag,
                         str_t**   rname,
                         uint64_t* mapq,
                         str_t**   rnext,
                         uint64_t* pnext,
                         uint64_t* tlen,
                         str_t**   opt)
{
    unsigned char  blk_id[8];
    uint64_t       blkl_n;
    uint64_t       data_sz;
    uint64_t       data_crc;
    unsigned char* data;

    /* Read and check block header */
    if (fread_buf(ifp, blk_id, sizeof(blk_id)) != sizeof(blk_id))
        tsc_error("Could not read block ID!\n");
    if (strncmp("aux----", (const char*)blk_id, 7))
        tsc_error("Wrong block ID: %s\n", blk_id);
    if (fread_uint64(ifp, &blkl_n) != sizeof(blkl_n))
        tsc_error("Could not read no. of lines in block!\n");
    if (fread_uint64(ifp, &data_sz) != sizeof(data_sz))
        tsc_error("Could not read data size!\n");
    if (fread_uint64(ifp, &data_crc) != sizeof(data_crc))
        tsc_error("Could not read CRC64 of compressed data!\n");

    tsc_vlog("Reading aux block: %zu lines, %zu data bytes\n", blkl_n,
             data_sz);

    /* Read and check block, then decompress and decode it. */
    data = (unsigned char*)tsc_malloc((size_t)data_sz);
    if (fread_buf(ifp, data, data_sz) != data_sz)
        tsc_error("Could not read aux block!\n");

    if (crc64(data, data_sz) != data_crc)
        tsc_error("CRC64 check failed for aux block!\n");

    unsigned int residues_sz = 0;
    unsigned char* residues = arith_decompress_o0(data, data_sz, &residues_sz);
    free(data);

    tsc_vlog("Decompressed aux block: %zu bytes -> %zu bytes (%5.2f%%)\n",
             data_sz, residues_sz, (double)residues_sz/(double)data_sz*100);

    auxdec_decode_block_o0(auxdec, residues, residues_sz, qname, flag, rname,
                           mapq, rnext, pnext, tlen, opt);

    tsc_vlog("Decoded aux block\n");

    free(residues); /* free memory used for decoded bitstream */
}

