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
#include <math.h>
#include <string.h>

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
static void qualenc_init(qualenc_t* qualenc, const unsigned int order)
{
    qualenc->order = order;
    qualenc->block_lc = 0;
}

qualenc_t* qualenc_new(const unsigned int order)
{
    qualenc_t* qualenc = (qualenc_t*)tsc_malloc_or_die(sizeof(qualenc_t));
    qualenc->qual_cbuf = cbufstr_new(QUALCODEC_WINDOW_SZ);
    qualenc->out_buf = str_new();
    qualenc_init(qualenc, order);
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

static int qualenc_match(str_t* ref, str_t* mem)
{
    char* match = strstr(ref->s, mem->s);
    if (match == NULL) return -1;
    int pos = match - ref->s;
    return pos;
}

static void qualenc_add_record_o0(qualenc_t* qualenc, const char* qual)
{
    /* Fall through. */
    /* Output record. */
    str_append_cstr(qualenc->out_buf, qual);
    str_append_cstr(qualenc->out_buf, "\n");
}

static void qualenc_add_record_o1(qualenc_t* qualenc, const char* qual)
{
    /* String matching. */

    if (qual[0] != '*' && qualenc->block_lc > 0) {
        /* QUAL exists and this is not the first line in the current block.
         * Encode it!
         */
        const size_t N = 3;
        size_t i = 0;

        /* Add first N quality characters to output buffer. */
        for (i = 0; i < N; i++) str_append_char(qualenc->out_buf, qual[i]);

        str_t* mem = str_new();
        size_t qual_len = strlen(qual);

        size_t j = 0;
        for (i = N; i < qual_len; i++) {
            /* Get memory values. */
            str_clear(mem);
            for (j = N; j > 0; j--) str_append_char(mem, qual[i - j]);

            /* Search all matches of mem in the circular buffer. */
            str_t* matches = str_new();
            size_t cbuf_idx = 0;
            size_t cbuf_n = qualenc->qual_cbuf->n;

            do {
                /* Get quality line from buffer. */
                str_t* ref = cbufstr_get(qualenc->qual_cbuf, cbuf_idx++);

                /* Search for match of mem. */
                int match_pos = qualenc_match(ref, mem);
                if (match_pos >= 0) str_append_char(matches, ref->s[match_pos]);
            } while (cbuf_idx < cbuf_n);

            /* Compute the prediction error and write it to output
             * buffer.
             */
            if (matches->n == 0) {
                /* Did not find any matches. Just write qual[i] to output. */
                str_append_char(qualenc->out_buf, qual[i]);
            } else {
                /* Compute mean prediction value. */
                int pred = 0;
                size_t m = 0;
                for (m = 0; m < matches->n; m++) pred += (int)(matches->s[m]);
                pred = (int)round((double)pred / (double)matches->n);
                char err = qual[i] - (char)pred;
                str_append_char(qualenc->out_buf, err);
            }

            str_free(matches);
        }
        str_free(mem);
        str_append_cstr(qualenc->out_buf, "\n");

        /* Push QUAL to circular buffer. */
        cbufstr_push(qualenc->qual_cbuf, qual);
    } else {
        /* QUAL is not present or this is the first line in the current
         * block.
         */

        /* Output record .*/
        str_append_cstr(qualenc->out_buf, qual);
        str_append_cstr(qualenc->out_buf, "\n");

        /* If this quality line is present, add it to circular buffer. */
        if (qual[0] != '*') cbufstr_push(qualenc->qual_cbuf, qual);
    }

    qualenc->block_lc++;
}

void qualenc_add_record(qualenc_t* qualenc, const char* qual)
{
    switch (qualenc->order) {
    case 0: /* fall through */
    case 1: qualenc_add_record_o0(qualenc, qual); break;
    case 2: /* fall through */
    case 3: qualenc_add_record_o1(qualenc, qual); break;
    default: tsc_error("Wrong quality score compression order: %zu\n", qualenc->order);
    }
}

static void qualenc_reset(qualenc_t* qualenc)
{
    qualenc->block_lc = 0;
    cbufstr_clear(qualenc->qual_cbuf);
    str_clear(qualenc->out_buf);
}

size_t qualenc_write_block(qualenc_t* qualenc, FILE* ofp)
{
    unsigned int ac_order = 0;
    switch (qualenc->order) {
    case 0: ac_order = 0; break;
    case 1: ac_order = 1; break;
    case 2: ac_order = 0; break;
    case 3: ac_order = 1; break;
    default: tsc_error("Wrong quality score compression order: %zu\n", qualenc->order);
    }

    /* Write block:
     * - unsigned char[8]: "QUAL----"
     * - uint32_t        : quality compression order
     * - uint32_t        : no. of lines in block
     * - uint32_t        : block size
     * - unsigned char[] : data
     */
    size_t header_byte_cnt = 0;
    size_t data_byte_cnt = 0;
    size_t byte_cnt = 0;

    header_byte_cnt += fwrite_cstr(ofp, "QUAL----");
    header_byte_cnt += fwrite_uint32(ofp, qualenc->order);
    header_byte_cnt += fwrite_uint32(ofp, qualenc->block_lc);

    /* Compress block with AC. */
    unsigned char* ac_in = (unsigned char*)qualenc->out_buf->s;
    unsigned int ac_in_sz = qualenc->out_buf->n;
    unsigned int ac_out_sz = 0;
    unsigned char* ac_out;
    if (ac_order == 0) ac_out = arith_compress_O0(ac_in, ac_in_sz, &ac_out_sz);
    if (ac_order == 1) ac_out = arith_compress_O1(ac_in, ac_in_sz, &ac_out_sz);
    if (tsc_verbose) tsc_log("Compressed QUAL block with AC: %zu bytes -> %zu bytes\n", ac_in_sz, ac_out_sz);

    /* Write compressed block to ofp. */
    header_byte_cnt += fwrite_uint32(ofp, ac_out_sz);
    data_byte_cnt += fwrite_buf(ofp, ac_out, ac_out_sz);
    free((void*)ac_out);
    byte_cnt = header_byte_cnt + data_byte_cnt;

    /* Log this if in verbose mode. */
    if (tsc_verbose)
        tsc_log("\tWrote QUAL block (order: %zu):\n"
                "\t  lines : %12zu\n"
                "\t  header: %12zu bytes\n"
                "\t  data  : %12zu bytes\n"
                "\t  total : %12zu bytes\n",
                (size_t)qualenc->order,
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
     * - uint32_t        : quality compression order
     * - uint32_t        : no. of lines in block
     * - uint32_t        : block size
     * - unsigned char[] : data
     */
    size_t header_byte_cnt = 0;
    size_t data_byte_cnt = 0;
    size_t byte_cnt = 0;

    /* Read and check block header. */
    unsigned char block_id[8];
    uint32_t order;
    uint32_t block_sz;

    if (fread_buf(ifp, block_id, sizeof(block_id)) != sizeof(block_id))
        tsc_error("Could not read block ID!\n");
    if (fread_uint32(ifp, &order) != sizeof(order))
        tsc_error("Could not read quality compression order!\n");
    if (fread_uint32(ifp, &(qualdec->block_lc)) != sizeof(qualdec->block_lc))
        tsc_error("Could not read no. of lines in block!\n");
    if (fread_uint32(ifp, &block_sz) != sizeof(block_sz))
        tsc_error("Could not read block size!\n");
    if (strncmp("QUAL----", (const char*)block_id, sizeof(block_id)))
        tsc_error("Wrong block ID: %s\n", block_id);

    header_byte_cnt += sizeof(block_id) + sizeof(order) + sizeof(qualdec->block_lc) + sizeof(block_sz);
    data_byte_cnt += block_sz;
    byte_cnt = header_byte_cnt + data_byte_cnt;

    /* Log this if in verbose mode. */
    if (tsc_verbose)
        tsc_log("\tReading QUAL block (order: %zu):\n"
                "\t  lines : %12zu\n"
                "\t  header: %12zu bytes\n"
                "\t  data  : %12zu bytes\n"
                "\t  total : %12zu bytes\n",
                (size_t)order,
                (size_t)qualdec->block_lc,
                header_byte_cnt,
                data_byte_cnt,
                byte_cnt);

    /* Read block. */
    bbuf_t* bbuf = bbuf_new();
    bbuf_reserve(bbuf, block_sz);
    fread_buf(ifp, bbuf->bytes, block_sz);

    /* Uncompress block with AC. */
    unsigned int ac_order = 0;
    switch (order) {
    case 0: ac_order = 0; break;
    case 1: ac_order = 1; break;
    case 2: ac_order = 0; break;
    case 3: ac_order = 1; break;
    default: tsc_error("Wrong quality score compression order: %zu\n", order);
    }

    unsigned char* ac_in = bbuf->bytes;
    unsigned int ac_in_sz = block_sz;
    unsigned int ac_out_sz = 0;
    unsigned char* ac_out;
    if (ac_order == 0) ac_out = arith_uncompress_O0(ac_in, ac_in_sz, &ac_out_sz);
    if (ac_order == 1) ac_out = arith_uncompress_O1(ac_in, ac_in_sz, &ac_out_sz);
    if (tsc_verbose) tsc_log("Uncompressed QUAL block with AC: %zu bytes -> %zu bytes\n", ac_in_sz, ac_out_sz);
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

