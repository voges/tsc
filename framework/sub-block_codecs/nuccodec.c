/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "nuccodec.h"
#include "../arithcodec/arithcodec.h"
#include "../crc64/crc64.h"
#include "../frw/frw.h"
#include "../tsclib.h"
#include <inttypes.h>
#include <string.h>

/*
 * Nuc block format:
 *   unsigned char  blk_id[8] : "nuc----" + '\0'
 *   uint64_t       blkl_n    : no. of lines in block
 *   uint64_t       data_sz   : data size
 *   uint64_t       data_crc  : CRC64 of compressed data
 *   unsigned char* data      : data
 */

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
static void nucenc_init(nucenc_t* nucenc)
{
    nucenc->blkl_n = 0;
}

nucenc_t* nucenc_new(void)
{
    nucenc_t* nucenc = (nucenc_t*)tsc_malloc(sizeof(nucenc_t));
    nucenc->residues = str_new();
    nucenc_init(nucenc);
    return nucenc;
}

void nucenc_free(nucenc_t* nucenc)
{
    if (nucenc != NULL) {
        str_free(nucenc->residues);
        free(nucenc);
        nucenc = NULL;
    } else { /* nucenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void nucenc_reset(nucenc_t* nucenc)
{
    nucenc_init(nucenc);
    str_clear(nucenc->residues);
}

/*
 * nucenc_add_record_o0: Fall-trough to entropy coder
 */
static void nucenc_add_record_o0(nucenc_t*      nucenc,
                                 const int64_t  pos,
                                 const char*    cigar,
                                 const char*    seq)
{
    char pos_cstr[101];
    snprintf(pos_cstr, sizeof(pos_cstr), "%"PRId64, pos);

    str_append_cstr(nucenc->residues, pos_cstr);
    str_append_cstr(nucenc->residues, "\t");
    str_append_cstr(nucenc->residues, cigar);
    str_append_cstr(nucenc->residues, "\t");
    str_append_cstr(nucenc->residues, seq);
    str_append_cstr(nucenc->residues, "\n");
}

void nucenc_add_record(nucenc_t*      nucenc,
                       const int64_t  pos,
                       const char*    cigar,
                       const char*    seq)
{
    nucenc_add_record_o0(nucenc, pos, cigar, seq);
    nucenc->blkl_n++;
}

size_t nucenc_write_block(nucenc_t* nucenc, FILE* ofp)
{
    size_t blk_sz = 0;

    /* Compress whole block with an entropy coder */
    unsigned char* residues = (unsigned char*)nucenc->residues->s;
    unsigned int residues_sz = (unsigned int)nucenc->residues->len;
    unsigned int data_sz = 0;
    unsigned char* data = arith_compress_o0(residues, residues_sz, &data_sz);

    tsc_vlog("Compressed nuc block: %zu bytes -> %zu bytes (%6.2f%%)\n",
             residues_sz, data_sz, (double)data_sz / (double)residues_sz*100);

    /* Compute CRC64 */
    uint64_t data_crc = crc64(data, data_sz);

    /* Write compressed block */
    unsigned char blk_id[8] = "nuc----"; blk_id[7] = '\0';
    blk_sz += fwrite_buf(ofp, blk_id, sizeof(blk_id));
    blk_sz += fwrite_uint64(ofp, (uint64_t)nucenc->blkl_n);
    blk_sz += fwrite_uint64(ofp, (uint64_t)data_sz);
    blk_sz += fwrite_uint64(ofp, (uint64_t)data_crc);
    blk_sz += fwrite_buf(ofp, data, data_sz);

    tsc_vlog("Wrote nuc block: %zu lines, %zu data bytes\n",
             nucenc->blkl_n, data_sz);

    nucenc_reset(nucenc); /* reset encoder for next block */
    free(data); /* free memory used for encoded bitstream */

    return blk_sz;
}

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
static void nucdec_init(nucdec_t* nucdec)
{

}

nucdec_t* nucdec_new(void)
{
    nucdec_t* nucdec = (nucdec_t*)tsc_malloc(sizeof(nucdec_t));
    nucdec_init(nucdec);
    return nucdec;
}

void nucdec_free(nucdec_t* nucdec)
{
    if (nucdec != NULL) {
        free(nucdec);
        nucdec = NULL;
    } else { /* nucdec == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void nucdec_reset(nucdec_t* nucdec)
{
    nucdec_init(nucdec);
}

static void nucdec_decode_block_o0(nucdec_t*      nucdec,
                                   unsigned char* residues,
                                   size_t         residues_sz,
                                   int64_t*       pos,
                                   str_t**        cigar,
                                   str_t**        seq)
{
    size_t i = 0;
    size_t line = 0;
    unsigned char* cstr = residues;
    unsigned int idx = 0;

    for (i = 0; i < residues_sz; i++) {
        if (residues[i] == '\n') {
            residues[i] = '\0';
            str_append_cstr(seq[line], (const char*)cstr);
            cstr = &residues[i+1];
            idx = 0;
            line++;
            continue;
        }

        if (residues[i] == '\t') {
            residues[i] = '\0';
            switch (idx++) {
            case 0:
                pos[line] = strtoll((const char*)cstr, NULL, 10);
                break;
            case 1:
                str_append_cstr(cigar[line], (const char*)cstr);
                break;
            default:
                tsc_error("Failed to decode nuc block (o0)!\n");
                break;
            }
            cstr = &residues[i+1];
        }
    }
}

void nucdec_decode_block(nucdec_t* nucdec,
                         FILE*     ifp,
                         int64_t*  pos,
                         str_t**   cigar,
                         str_t**   seq)
{
    unsigned char  blk_id[8];
    uint64_t       blkl_n;
    uint64_t       data_sz;
    uint64_t       data_crc;
    unsigned char* data;

    /* Read and check block header */
    if (fread_buf(ifp, blk_id, sizeof(blk_id)) != sizeof(blk_id))
        tsc_error("Could not read block ID!\n");
    if (strncmp("nuc----", (const char*)blk_id, 7))
        tsc_error("Wrong block ID: %s\n", blk_id);
    if (fread_uint64(ifp, &blkl_n) != sizeof(blkl_n))
        tsc_error("Could not read no. of lines in block!\n");
    if (fread_uint64(ifp, &data_sz) != sizeof(data_sz))
        tsc_error("Could not read data size!\n");
    if (fread_uint64(ifp, &data_crc) != sizeof(data_crc))
        tsc_error("Could not read CRC64 of compressed data!\n");

    tsc_vlog("Reading nuc block: %zu lines, %zu data bytes\n", blkl_n, data_sz);

    /* Read and check block, then decompress and decode it. */
    data = (unsigned char*)tsc_malloc((size_t)data_sz);
    if (fread_buf(ifp, data, data_sz) != data_sz)
        tsc_error("Could not read nuc block!\n");

    if (crc64(data, data_sz) != data_crc)
        tsc_error("CRC64 check failed for nuc block!\n");

    unsigned int residues_sz = 0;
    unsigned char* residues = arith_decompress_o0(data, data_sz, &residues_sz);
    free(data);

    tsc_vlog("Decompressed nuc block: %zu bytes -> %zu bytes (%5.2f%%)\n",
             data_sz, residues_sz, (double)residues_sz/(double)data_sz*100);

    nucdec_decode_block_o0(nucdec, residues, residues_sz, pos, cigar, seq);

    tsc_vlog("Decoded nuc block\n");

    free(residues); /* free memory used for decoded bitstream */

    nucdec_reset(nucdec);
}

