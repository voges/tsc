/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "nuccodec.h"
#include "arithcodec.h"
#include "bbuf.h"
#include "frw.h"
#include "tsclib.h"
#include <ctype.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
static void nucenc_init(nucenc_t* nucenc)
{
    nucenc->block_lc = 0;
}

nucenc_t* nucenc_new(void)
{
    nucenc_t* nucenc = (nucenc_t*)tsc_malloc(sizeof(nucenc_t));
    nucenc->out_buf = str_new();
    nucenc_init(nucenc);
    return nucenc;
}

void nucenc_free(nucenc_t* nucenc)
{
    if (nucenc != NULL) {;
        str_free(nucenc->out_buf);
        free((void*)nucenc);
        nucenc = NULL;
    } else { /* nucenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

void nucenc_add_record(nucenc_t*      nucenc,
                       const uint64_t pos,
                       const char*    cigar,
                       const char*    seq)
{    
    if (pos > pow(10, 10) - 1) tsc_error("Buffer too small for POS data!\n");
    char tmp[101]; snprintf(tmp, sizeof(tmp), "%"PRIu64, pos);

    str_append_cstr(nucenc->out_buf, tmp);
    str_append_cstr(nucenc->out_buf, "\t");
    str_append_cstr(nucenc->out_buf, cigar);
    str_append_cstr(nucenc->out_buf, "\t");
    str_append_cstr(nucenc->out_buf, seq);
    str_append_cstr(nucenc->out_buf, "\n");

    nucenc->block_lc++;
}

static void nucenc_reset(nucenc_t* nucenc)
{
    nucenc->block_lc = 0;
    str_clear(nucenc->out_buf);
}

size_t nucenc_write_block(nucenc_t* nucenc, FILE* ofp)
{
    /* Write block:
     * - unsigned char[8]: "NUC-----"
     * - uint32_t        : no. of lines in block
     * - uint32_t        : block size
     * - unsigned char[] : data
     */
    size_t header_byte_cnt = 0;
    size_t data_byte_cnt = 0;
    size_t byte_cnt = 0;

    header_byte_cnt += fwrite_cstr(ofp, "NUC-----");
    header_byte_cnt += fwrite_uint32(ofp, nucenc->block_lc);

    /* Compress block with AC. */
    unsigned char* ac_in = (unsigned char*)nucenc->out_buf->s;
    unsigned int ac_in_sz = nucenc->out_buf->n;
    unsigned int ac_out_sz = 0;
    unsigned char* ac_out = arithcodec_compress_O0(ac_in, ac_in_sz, &ac_out_sz);
    //unsigned char* ac_out = arithcodec_compress_O1(ac_in, ac_in_sz, &ac_out_sz);
    if (tsc_verbose) tsc_log("Compressed NUC block with AC: %zu bytes -> %zu bytes\n", ac_in_sz, ac_out_sz);

    /* Write compressed block to ofp. */
    header_byte_cnt += fwrite_uint32(ofp, ac_out_sz);
    data_byte_cnt += fwrite_buf(ofp, ac_out, ac_out_sz);
    free((void*)ac_out);
    byte_cnt = header_byte_cnt + data_byte_cnt;

    /* Log this if in verbose mode. */
    if (tsc_verbose)
        tsc_log("\tWrote NUC block:\n"
                "\t  lines : %12zu\n"
                "\t  header: %12zu bytes\n"
                "\t  data  : %12zu bytes\n"
                "\t  total : %12zu bytes\n",
                (size_t)nucenc->block_lc,
                header_byte_cnt,
                data_byte_cnt,
                byte_cnt);

    /* Reset encoder for next block. */
    nucenc_reset(nucenc);
    
    return byte_cnt;
}

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
static void nucdec_init(nucdec_t* nucdec)
{
    nucdec->block_lc = 0;
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
        free((void*)nucdec);
        nucdec = NULL;
    } else { /* nucdec == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void nucdec_reset(nucdec_t* nucdec)
{
    nucdec->block_lc = 0;
}

void nucdec_decode_block(nucdec_t* nucdec,
                         FILE*     ifp,
                         uint64_t* pos,
                         str_t**   cigar,
                         str_t**   seq)
{
    /* Read block:
     * - unsigned char[8]: "NUC-----"
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
    if (fread_uint32(ifp, &(nucdec->block_lc)) != sizeof(nucdec->block_lc))
        tsc_error("Could not read no. of lines in block!\n");
    if (fread_uint32(ifp, &block_sz) != sizeof(block_sz))
        tsc_error("Could not read block size!\n");
    if (strncmp("NUC-----", (const char*)block_id, sizeof(block_id)))
        tsc_error("Wrong block ID: %s\n", block_id);

    header_byte_cnt += sizeof(block_id) + sizeof(nucdec->block_lc) + sizeof(block_sz);
    data_byte_cnt += block_sz;
    byte_cnt = header_byte_cnt + data_byte_cnt;

    /* Log this if in verbose mode. */
    if (tsc_verbose)
        tsc_log("\tReading NUC block:\n"
                "\t  lines : %12zu\n"
                "\t  header: %12zu bytes\n"
                "\t  data  : %12zu bytes\n"
                "\t  total : %12zu bytes\n",
                (size_t)nucdec->block_lc,
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
    unsigned char* ac_out = arithcodec_uncompress_O0(ac_in, ac_in_sz, &ac_out_sz);
    //unsigned char* ac_out = arithcodec_uncompress_O1(ac_in, ac_in_sz, &ac_out_sz);
    if (tsc_verbose) tsc_log("Uncompressed NUC block with AC: %zu bytes -> %zu bytes\n", ac_in_sz, ac_out_sz);
    bbuf_free(bbuf);

    /* Decode block. */
    str_t* tmp = str_new();
    size_t i = 0;
    size_t line = 0;
    size_t field = 0;
    for (i = 0; i < ac_out_sz; i++) {
        switch (ac_out[i]) {
        case '\n': {
            pos[line++] = atoi(tmp->s);
            str_clear(tmp);
            field = 0;
            break;
        }
        case '\t': field++; continue;
        default: {
            switch (field) {
            case 0: str_append_char(tmp, (const char)ac_out[i]); break;
            case 1: str_append_char(cigar[line], (const char)ac_out[i]); break;
            case 2: str_append_char(seq[line], (const char)ac_out[i]); break;
            }
        }
        }
    }

    str_free(tmp);
    free((void*)ac_out);

    /* Reset decoder for next block. */
    nucdec_reset(nucdec);
}

