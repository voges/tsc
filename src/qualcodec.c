/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "qualcodec.h"
#include "../arithcodec/arithcodec.h"
#include "bbuf.h"
#include "frw.h"
#include "../ricecodec/ricecodec.h"
#include "tsclib.h"
#include <float.h>
#include <math.h>
#include <string.h>

/******************************************************************************
 * Encoder                                                                    *
 ******************************************************************************/
static void qualenc_init_tables(qualenc_t* qualenc)
{
    size_t i = 0, j = 0;
    char pred_init = 0;
    for (i = 0; i < QUALCODEC_TABLE_SZ; i++) {
        qualenc->pred[i] = pred_init++;
        if (pred_init == QUALCODEC_RANGE) pred_init = 0;
        for (j = 0; j < QUALCODEC_RANGE; j++) {
            qualenc->freq[i][j] = 0;
        }
    }
}

static void qualenc_init(qualenc_t* qualenc)
{
    qualenc->block_lc = 0;
    qualenc_init_tables(qualenc);
}

qualenc_t* qualenc_new(void)
{
    qualenc_t* qualenc = (qualenc_t*)tsc_malloc(sizeof(qualenc_t));
    qualenc->qual_cbuf = cbufstr_new(QUALCODEC_WINDOW_SZ);
    qualenc->out_buf = str_new();
    qualenc->qual_cbuf_len = cbufint64_new(QUALCODEC_WINDOW_SZ);
    qualenc->qual_cbuf_mu = cbufint64_new(QUALCODEC_WINDOW_SZ);
    qualenc->qual_cbuf_var = cbufint64_new(QUALCODEC_WINDOW_SZ);
    qualenc_init(qualenc);
    return qualenc;
}

void qualenc_free(qualenc_t* qualenc)
{
    if (qualenc != NULL) {
        cbufstr_free(qualenc->qual_cbuf);
        str_free(qualenc->out_buf);
        cbufint64_free(qualenc->qual_cbuf_len);
        cbufint64_free(qualenc->qual_cbuf_mu);
        cbufint64_free(qualenc->qual_cbuf_var);
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
    /* Nothing (only EC). */
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
            if (matches->len == 0) {
                /* Did not find any matches. Just write qual[i] to output. */
                str_append_char(qualenc->out_buf, qual[i]);
            } else {
                /* Compute mean prediction value. */
                int pred = 0;
                size_t m = 0;
                for (m = 0; m < matches->len; m++) pred += (int)(matches->s[m]);
                pred = (int)round((double)pred / (double)matches->len);
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

static int qualenc_mu(const char* qual)
{
    size_t len = strlen(qual);
    double mu = 0;
    size_t i = 0;
    for (i = 0; i < len; i++) mu += (double)qual[i];
    return (int)(mu / (double)len);
}

static int qualenc_var(const char* qual)
{
    int mu = qualenc_mu(qual);
    size_t len = strlen(qual);
    double var = 0;
    size_t i = 0;
    for (i = 0; i < len; i++)
        var += ((double)qual[i] - (double)mu) * ((double)qual[i] - (double)mu);
    return (int)(var / (double)len);
}

static void qualenc_add_record_o2(qualenc_t* qualenc, const char* qual)
{
    /* Line context. */

    /* Assuming 33 <= QUAL <= 126 */
    DEBUG("%s", qual);
    char* q = (char*)tsc_malloc(strlen(qual));
    size_t itr = 0;
    for (itr = 0; itr < strlen(qual); itr++) {
        if (qual[itr] < QUALCODEC_MIN || qual[itr] > QUALCODEC_MAX)
            tsc_error("Quality score is out of range: %c\n", qual[itr]);
        else
            q[itr] -= QUALCODEC_MIN;
    }
    DEBUG("range ok");
    DEBUG("%s", q);

    if (qual[0] != '*' && qualenc->block_lc > 0) {
        /* QUAL exists and this is not the first line in the current block.
         * Encode it!
         */
        size_t i = 0, j = 0;

        /* Compute prediction errors for the first symbols. */
        for (i = 0; i < QUALCODEC_MEM_SZ; i++) {
            char err = q[i] - qualenc->pred[q[i] * q[i] * q[i]];
            str_append_char(qualenc->out_buf, err);
        }

        /* Compute prediction errors for the other symbols. */
        size_t curr_len = strlen(qual);
        int curr_mu = qualenc_mu(q);
        int curr_var = qualenc_var(q);

        size_t cbuf_idx = 0;
        size_t cbuf_idx_best = 0;
        size_t cbuf_n = qualenc->qual_cbuf->n;
        double d = 0, d_best = DBL_MAX;
        do {
            /* Get quality line from buffer. */
            int64_t len = cbufint64_get(qualenc->qual_cbuf_len, cbuf_idx);
            int64_t mu = cbufint64_get(qualenc->qual_cbuf_mu, cbuf_idx);
            int64_t var = cbufint64_get(qualenc->qual_cbuf_var, cbuf_idx);

            /* Compute distance. */
            d = (curr_len - len) * (curr_len - len);
            d += (curr_mu - mu) * (curr_mu - mu);
            d += (curr_var - var) * (curr_var - var);
            if (d < d_best) {
                d_best = d;
                cbuf_idx_best = cbuf_idx;
            }

            cbuf_idx++;
        } while (cbuf_idx < cbuf_n);

        /* Get best reference line. */
        str_t* ref = cbufstr_get(qualenc->qual_cbuf, cbuf_idx_best);
        DEBUG("ref: %s", ref->s);

        for (i = QUALCODEC_MEM_SZ; i < strlen(qual); i++) {
            /* Compute the prediction error and write it to output
             * buffer.
             */
            size_t table_idx = q[i-QUALCODEC_MEM_SZ] * q[i-QUALCODEC_MEM_SZ+1] * ref->s[i];
            char err = q[i] - qualenc->pred[table_idx];
            str_append_char(qualenc->out_buf, err);

            /* Update tables. */
            (qualenc->freq[table_idx][(size_t)q[i]])++;

            unsigned int max = 0;
            for (j = 0; j < QUALCODEC_RANGE; j++) {
                if (qualenc->freq[table_idx][j] > max)
                    max = j;
            }
            qualenc->pred[table_idx] = max;
        }
        str_append_cstr(qualenc->out_buf, "\n");

        /* Push QUAL and it len, mu, and var to circular buffers. */
        cbufstr_push(qualenc->qual_cbuf, q);
        cbufint64_push(qualenc->qual_cbuf_len, strlen(qual));
        cbufint64_push(qualenc->qual_cbuf_mu, (int64_t)qualenc_mu(q));
        cbufint64_push(qualenc->qual_cbuf_var, (int64_t)qualenc_var(q));
    } else {
        /* QUAL is not present or this is the first line in the current
         * block.
         */

        /* Output record .*/
        str_append_cstr(qualenc->out_buf, qual);
        str_append_cstr(qualenc->out_buf, "\n");

        /* If this quality line is present, add it to circular buffer. */
        if (qual[0] != '*') {
            cbufstr_push(qualenc->qual_cbuf, q);
            cbufint64_push(qualenc->qual_cbuf_len, strlen(qual));
            cbufint64_push(qualenc->qual_cbuf_mu, (int64_t)qualenc_mu(q));
            cbufint64_push(qualenc->qual_cbuf_var, (int64_t)qualenc_var(q));
        }
    }

    qualenc->block_lc++;
}

void qualenc_add_record(qualenc_t* qualenc, const char* qual)
{
    qualenc_add_record_o0(qualenc, qual); /* nothing (only EC) */
    //qualenc_add_record_o1(qualenc, qual); /* string matching */
    //qualenc_add_record_o2(qualenc, qual); /* line context */
}

static void qualenc_reset(qualenc_t* qualenc)
{
    qualenc->block_lc = 0;
    cbufstr_clear(qualenc->qual_cbuf);
    str_clear(qualenc->out_buf);
    cbufint64_clear(qualenc->qual_cbuf_len);
    cbufint64_clear(qualenc->qual_cbuf_mu);
    cbufint64_clear(qualenc->qual_cbuf_var);
    qualenc_init_tables(qualenc);
}

size_t qualenc_write_block(qualenc_t* qualenc, FILE* ofp)
{
    /* Write block:
     * - unsigned char[8]: "QUAL----"
     * - uint32_t        : no. of lines in block
     * - uint32_t        : block size
     *   uint64_t        : CRC64
     * - unsigned char[] : data
     */
    size_t header_byte_cnt = 0;
    size_t data_byte_cnt = 0;
    size_t byte_cnt = 0;

    header_byte_cnt += fwrite_cstr(ofp, "QUAL----");
    header_byte_cnt += fwrite_uint32(ofp, qualenc->block_lc);

    /* Compress block with AC or Rice coder. */
    unsigned char* ec_in = (unsigned char*)qualenc->out_buf->s;
    size_t ec_in_sz = qualenc->out_buf->len;
    unsigned int ec_out_sz = 0;
    unsigned char* ec_out;
    //ec_out = arith_compress_o0(ec_in, ec_in_sz, &ec_out_sz);
    //ec_out = arith_compress_o1(ec_in, ec_in_sz, &ec_out_sz);

    // Testing Rice codec
    size_t i = 0;
    for (i = 0; i < ec_in_sz; i++) ec_in[i] = 0;

    ec_out = rice_compress(ec_in, ec_in_sz, &ec_out_sz);
    if (tsc_verbose) tsc_log("Compressed QUAL block: %zu bytes -> %zu bytes\n", ec_in_sz, ec_out_sz);

    /* Write compressed block to ofp. */
    header_byte_cnt += fwrite_uint32(ofp, ec_out_sz);
    data_byte_cnt += fwrite_buf(ofp, ec_out, ec_out_sz);
    free((void*)ec_out);
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
    qualdec_t* qualdec = (qualdec_t*)tsc_malloc(sizeof(qualdec_t));
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
    uint32_t order;
    uint32_t block_sz;

    if (fread_buf(ifp, block_id, sizeof(block_id)) != sizeof(block_id))
        tsc_error("Could not read block ID!\n");
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

    /* Uncompress block with AC or Rice coder. */
    unsigned char* ec_in = bbuf->bytes;
    unsigned int ec_in_sz = block_sz;
    unsigned int ec_out_sz = 0;
    unsigned char* ec_out;
    ec_out = arith_decompress_o0(ec_in, ec_in_sz, &ec_out_sz);
    //ec_out = arith_decompress_o1(ec_in, ec_in_sz, &ec_out_sz);
    //ec_out = rice_decompress(ec_in, ec_in_sz, &ec_out_sz);
    if (tsc_verbose) tsc_log("Decompressed QUAL block: %zu bytes -> %zu bytes\n", ec_in_sz, ec_out_sz);
    bbuf_free(bbuf);

    /* Decode block. */
    DEBUG("Printing decoded QUAL block.\n");
    size_t i = 0;
    size_t line = 0;
    for (i = 0; i < ec_out_sz; i++) {
        printf("%c ", ec_out[i]);
        if (ec_out[i] != '\n')
            str_append_char(qual[line], (const char)ec_out[i]);
        else
            line++;
    }

    free((void*)ec_out);

    /* Reset decoder for next block. */
    qualdec_reset(qualdec);
}

