/*
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>
 *
 * This file is part of tsc.
 */

/*
 * Qual o0 encoder: Fall-through to entropy coder
 *
 * Qual o0 block format:
 *   unsigned char  blk_id[8] : "qual---" + '\0'
 *   uint64_t       blkl_n    : no. of lines in block
 *   uint64_t       data_sz   : data size
 *   uint64_t       data_crc  : CRC64 of compressed data
 *   unsigned char* data      : data
 */

#include "qualcodec.h"

#include "../rangecodec/rangecodec.h"
#include "../crc64/crc64.h"
#include "../frw/frw.h"
#include "../tsclib.h"
#include <string.h>

static void qualenc_init(qualenc_t* qualenc)
{
    qualenc->blkl_n = 0;
}

qualenc_t* qualenc_new(void)
{
    qualenc_t* qualenc = (qualenc_t*)tsc_malloc(sizeof(qualenc_t));
    qualenc->residues = str_new();
    qualenc_init(qualenc);
    return qualenc;
}

void qualenc_free(qualenc_t* qualenc)
{
    if (qualenc != NULL) {
        str_free(qualenc->residues);
        free(qualenc);
        qualenc = NULL;
    } else { /* qualenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void qualenc_reset(qualenc_t* qualenc)
{
    str_clear(qualenc->residues);
    qualenc_init(qualenc);
}

void qualenc_add_record(qualenc_t* qualenc, const char* qual)
{
    str_append_cstr(qualenc->residues, qual);
    str_append_cstr(qualenc->residues, "\n");

    qualenc->blkl_n++;
}

size_t qualenc_write_block(qualenc_t* qualenc, FILE* ofp)
{
    size_t blk_sz = 0;

    /* Compress whole block with an entropy coder */
    unsigned char* residues = (unsigned char*)qualenc->residues->s;
    unsigned int residues_sz = (unsigned int)qualenc->residues->len;
    unsigned int data_sz = 0;
    unsigned char* data = range_compress_o1(residues, residues_sz, &data_sz);

    tsc_vlog("Compressed qual block: %zu bytes -> %zu bytes (%6.2f%%)\n",
             residues_sz, data_sz, (double)data_sz / (double)residues_sz*100);

    /* Compute CRC64 */
    uint64_t data_crc = crc64(data, data_sz);

    /* Write compressed block */
    unsigned char blk_id[8] = "qual---"; blk_id[7] = '\0';
    blk_sz += fwrite_buf(ofp, blk_id, sizeof(blk_id));
    blk_sz += fwrite_uint64(ofp, (uint64_t)qualenc->blkl_n);
    blk_sz += fwrite_uint64(ofp, (uint64_t)data_sz);
    blk_sz += fwrite_uint64(ofp, (uint64_t)data_crc);
    blk_sz += fwrite_buf(ofp, data, data_sz);

    tsc_vlog("Wrote qual block: %zu lines, %zu data bytes\n",
             qualenc->blkl_n, data_sz);

    qualenc_reset(qualenc); /* reset encoder for next block */
    free(data); /* free memory used for encoded bitstream */

    return blk_sz;
}

static void qualdec_init(qualdec_t* qualdec)
{

}

qualdec_t* qualdec_new(void)
{
    qualdec_t* qualdec = (qualdec_t*)tsc_malloc(sizeof(qualdec_t));
    qualdec_init(qualdec);
    return qualdec;
}

void qualdec_free(qualdec_t* qualdec)
{
    if (qualdec != NULL) {
        free(qualdec);
        qualdec = NULL;
    } else { /* qualdec == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void qualdec_reset(qualdec_t* qualdec)
{
    qualdec_init(qualdec);
}

static void qualdec_decode_residues(qualdec_t*     qualdec,
                                    unsigned char* residues,
                                    size_t         residues_sz,
                                    str_t**        qual)
{
    size_t i = 0;
    size_t line = 0;
    for (i = 0; i < residues_sz; i++) {
        if (residues[i] != '\n')
            str_append_char(qual[line], (const char)residues[i]);
        else
            line++;
    }
}

void qualdec_decode_block(qualdec_t* qualdec, FILE* ifp, str_t** qual)
{
    unsigned char  blk_id[8];
    uint64_t       blkl_n;
    uint64_t       data_sz;
    uint64_t       data_crc;
    unsigned char* data;

    /* Read and check block header */
    if (fread_buf(ifp, blk_id, sizeof(blk_id)) != sizeof(blk_id))
        tsc_error("Could not read block ID!\n");
    if (strncmp("qual---", (const char*)blk_id, 7))
        tsc_error("Wrong block ID: %s\n", blk_id);
    if (fread_uint64(ifp, &blkl_n) != sizeof(blkl_n))
        tsc_error("Could not read no. of lines in block!\n");
    if (fread_uint64(ifp, &data_sz) != sizeof(data_sz))
        tsc_error("Could not read data size!\n");
    if (fread_uint64(ifp, &data_crc) != sizeof(data_crc))
        tsc_error("Could not read CRC64 of compressed data!\n");

    tsc_vlog("Reading qual block: %zu lines, %zu data bytes\n",
             blkl_n, data_sz);

    /* Read and check block, then decompress and decode it. */
    data = (unsigned char*)tsc_malloc((size_t)data_sz);
    if (fread_buf(ifp, data, data_sz) != data_sz)
        tsc_error("Could not read qual block!\n");

    if (crc64(data, data_sz) != data_crc)
        tsc_error("CRC64 check failed for qual block!\n");

    unsigned int residues_sz = 0;
    unsigned char* residues = range_decompress_o1(data, data_sz, &residues_sz);
    free(data);

    tsc_vlog("Decompressed qual block: %zu bytes -> %zu bytes (%5.2f%%)\n",
             data_sz, residues_sz, (double)residues_sz/(double)data_sz*100);

    qualdec_decode_residues(qualdec, residues, residues_sz, qual);

    tsc_vlog("Decoded qual block\n");

    free(residues); /* free memory used for decoded bitstream */

    qualdec_reset(qualdec);
}

