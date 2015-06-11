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
#include "ricecodec.h"
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

static void qualenc_init_tables(qualenc_t* qualenc)
{

}

qualenc_t* qualenc_new(const unsigned int order)
{
    qualenc_t* qualenc = (qualenc_t*)tsc_malloc_or_die(sizeof(qualenc_t));
    qualenc->qual_cbuf = cbufstr_new(QUALCODEC_WINDOW_SZ);
    qualenc->out_buf = str_new();
    qualenc->qual_cbuf_len = cbufint64_new(QUALCODEC_WINDOW_SZ);
    qualenc->qual_cbuf_mu = cbufint64_new(QUALCODEC_WINDOW_SZ);
    qualenc->qual_cbuf_var = cbufint64_new(QUALCODEC_WINDOW_SZ);
    memset(qualenc->freq, 0x00, sizeof(qualenc->freq));
    memset(qualenc->pred, 0x00, sizeof(qualenc->pred));
    qualenc_init(qualenc, order);
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

    if (qual[0] != '*' && qualenc->block_lc > 0) {
        /* QUAL exists and this is not the first line in the current block.
         * Encode it!
         */
        const size_t N = 2;
        size_t i = 0;

        str_t* mem = str_new();
        size_t qual_len = strlen(qual);

        size_t j = 0;
        for (i = N; i < qual_len; i++) {
            /* Get memory values. */
            str_clear(mem);
            for (j = N; j > 0; j--) str_append_char(mem, qual[i - j]);

            size_t cbuf_idx = 0;
            size_t cbuf_n = qualenc->qual_cbuf->n;

            do {
                /* Get quality line from buffer. */
                str_t* ref = cbufstr_get(qualenc->qual_cbuf, cbuf_idx++);


            } while (cbuf_idx < cbuf_n);

            /* Compute the prediction error and write it to output
             * buffer.
             */
            int pred = 0;
            char err = qual[i] - (char)pred;
            str_append_char(qualenc->out_buf, err);
        }
        str_free(mem);
        str_append_cstr(qualenc->out_buf, "\n");

        /* Push QUAL and it len, mu, and var to circular buffers. */
        cbufstr_push(qualenc->qual_cbuf, qual);
        cbufint64_push(qualenc->qual_cbuf_len, strlen(qual));
        cbufint64_push(qualenc->qual_cbuf_mu, (int64_t)qualenc_mu(qual));
        cbufint64_push(qualenc->qual_cbuf_var, (int64_t)qualenc_var(qual));
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
    case 1: /* fall through */
    case 2: qualenc_add_record_o0(qualenc, qual); break; /* nothing (only EC) */
    case 3: /* fall through */
    case 4: /* fall through */
    case 5: qualenc_add_record_o1(qualenc, qual); break; /* string matching */
    case 6: /* fall through */
    case 7: /* fall through */
    case 8: qualenc_add_record_o2(qualenc, qual); break; /* line context */
    default: tsc_error("Wrong quality score compression order: %zu\n", qualenc->order);
    }
}

static void qualenc_reset(qualenc_t* qualenc)
{
    qualenc->block_lc = 0;
    cbufstr_clear(qualenc->qual_cbuf);
    str_clear(qualenc->out_buf);
    cbufint64_clear(qualenc->qual_cbuf_len);
    cbufint64_clear(qualenc->qual_cbuf_mu);
    cbufint64_clear(qualenc->qual_cbuf_var);
    memset(qualenc->freq, 0x00, sizeof(qualenc->freq));
    memset(qualenc->pred, 0x00, sizeof(qualenc->pred));
    qualenc_init_tables(qualenc);
}

size_t qualenc_write_block(qualenc_t* qualenc, FILE* ofp)
{
    unsigned int ec_type = 0;
    switch (qualenc->order) {
    case 0: ec_type = 0; break; /* order-0 AC */
    case 1: ec_type = 1; break; /* order-1 AC */
    case 2: ec_type = 2; break; /* Rice coder */
    case 3: ec_type = 0; break; /* order-0 AC */
    case 4: ec_type = 1; break; /* order-1 AC */
    case 5: ec_type = 2; break; /* Rice coder */
    case 6: ec_type = 0; break; /* order-0 AC */
    case 7: ec_type = 1; break; /* order-1 AC */
    case 8: ec_type = 2; break; /* Rice coder */
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

    /* Compress block with AC or Rice coder. */
    unsigned char* ec_in = (unsigned char*)qualenc->out_buf->s;
    unsigned int ec_in_sz = qualenc->out_buf->n;
    unsigned int ec_out_sz = 0;
    unsigned char* ec_out;
    if (ec_type == 0) {
        ec_out = arith_compress_O0(ec_in, ec_in_sz, &ec_out_sz);
        if (tsc_verbose) tsc_log("Compressed QUAL block with order-0 AC: %zu bytes -> %zu bytes\n", ec_in_sz, ec_out_sz);
    }
    if (ec_type == 1) {
        ec_out = arith_compress_O1(ec_in, ec_in_sz, &ec_out_sz);
        if (tsc_verbose) tsc_log("Compressed QUAL block with order-1 AC: %zu bytes -> %zu bytes\n", ec_in_sz, ec_out_sz);
    }
    if (ec_type == 2) {
        ec_out = ricecodec_compress(ec_in, ec_in_sz, &ec_out_sz);
        if (tsc_verbose) tsc_log("Compressed QUAL block with RC: %zu bytes -> %zu bytes\n", ec_in_sz, ec_out_sz);
    }

    /* Write compressed block to ofp. */
    header_byte_cnt += fwrite_uint32(ofp, ec_out_sz);
    data_byte_cnt += fwrite_buf(ofp, ec_out, ec_out_sz);
    free((void*)ec_out);
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

    /* Uncompress block with AC or Rice coder. */
    unsigned int ec_type = 0;
    switch (order) {
    case 0: ec_type = 0; break; /* order-0 AC */
    case 1: ec_type = 1; break; /* order-1 AC */
    case 2: ec_type = 2; break; /* Rice coder */
    case 3: ec_type = 0; break; /* order-0 AC */
    case 4: ec_type = 1; break; /* order-1 AC */
    case 5: ec_type = 2; break; /* Rice coder */
    case 6: ec_type = 0; break; /* order-0 AC */
    case 7: ec_type = 1; break; /* order-1 AC */
    case 8: ec_type = 2; break; /* Rice coder */
    default: tsc_error("Wrong quality score compression order: %zu\n", order);
    }

    unsigned char* ec_in = bbuf->bytes;
    unsigned int ec_in_sz = block_sz;
    unsigned int ec_out_sz = 0;
    unsigned char* ec_out;
    if (ec_type == 0) {
        ec_out = arith_compress_O0(ec_in, ec_in_sz, &ec_out_sz);
        if (tsc_verbose) tsc_log("Deompressed QUAL block with order-0 AC: %zu bytes -> %zu bytes\n", ec_in_sz, ec_out_sz);
    }
    if (ec_type == 1) {
        ec_out = arith_compress_O1(ec_in, ec_in_sz, &ec_out_sz);
        if (tsc_verbose) tsc_log("Deompressed QUAL block with order-1 AC: %zu bytes -> %zu bytes\n", ec_in_sz, ec_out_sz);
    }
    if (ec_type == 2) {
        ec_out = ricecodec_compress(ec_in, ec_in_sz, &ec_out_sz);
        if (tsc_verbose) tsc_log("Deompressed QUAL block with RC: %zu bytes -> %zu bytes\n", ec_in_sz, ec_out_sz);
    }
    bbuf_free(bbuf);

    /* Decode block. */
    size_t i = 0;
    size_t line = 0;
    for (i = 0; i < ec_out_sz; i++) {
        if (ec_out[i] != '\n')
            str_append_char(qual[line], (const char)ec_out[i]);
        else
            line++;
    }

    free((void*)ec_out);

    /* Reset decoder for next block. */
    qualdec_reset(qualdec);
}

