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

/*
 * Qual o1 encoder: "String matching" (patent pending)
 *
 * Qual o1 block format:
 *   see o0
 */

/*
 * Qual o2 encoder: Markov model + "line context"
 *
 * Qual o2 block format:
 *   see o0
 */

#include "qualcodec.h"

#ifdef QUALCODEC_O0

#include "../arithcodec/arithcodec.h"
#include "../crc64/crc64.h"
#include "../frw/frw.h"
#include "../tsclib.h"
#include <string.h>

#endif /* QUALCODEC_O0 */

#ifdef QUALCODEC_O1

#include "../arithcodec/arithcodec.h"
#include "../crc64/crc64.h"
#include "../frw/frw.h"
#include "../ricecodec/ricecodec.h"
#include "../tsclib.h"
#include <limits.h>
#include <string.h>

#define QUALCODEC_EMPTY -126
#define QUALCODEC_LINEBREAK -125

#endif /* QUALCODEC_O1 */

#ifdef QUALCODEC_O2

#include "../arithcodec/arithcodec.h"
#include "../crc64/crc64.h"
#include "../frw/frw.h"
#include "../ricecodec/ricecodec.h"
#include "../tsclib.h"
#include <float.h>
#include <math.h>
#include <string.h>

#endif /* QUALCODEC_O2 */

#ifdef QUALCODEC_O1

static int qualcodec_match(str_t* ref, str_t* mem)
{
    char* match = strstr(ref->s, mem->s);
    if (match == NULL) return -1;
    int pos = match - ref->s;
    return pos;
}

#endif /* QUALCODEC_O1 */

/*
 * Encoder
 * -----------------------------------------------------------------------------
 */

#ifdef QUALCODEC_O0

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

#endif /* QUALCODEC_O0 */

#ifdef QUALCODEC_O1

static void qualenc_init(qualenc_t* qualenc)
{
    qualenc->blkl_n = 0;
}

qualenc_t* qualenc_new(void)
{
    qualenc_t* qualenc = (qualenc_t*)tsc_malloc(sizeof(qualenc_t));
    qualenc->qual_cbuf = cbufstr_new(QUALCODEC_WINDOW_SZ);
    qualenc->residues = str_new();
    qualenc_init(qualenc);
    return qualenc;
}

void qualenc_free(qualenc_t* qualenc)
{
    if (qualenc != NULL) {
        cbufstr_free(qualenc->qual_cbuf);
        str_free(qualenc->residues);
        free(qualenc);
        qualenc = NULL;
    } else { /* qualenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void qualenc_reset(qualenc_t* qualenc)
{
    cbufstr_clear(qualenc->qual_cbuf);
    str_clear(qualenc->residues);
    qualenc_init(qualenc);
}

void qualenc_add_record(qualenc_t* qualenc, const char* qual)
{
    /* We assume 33 <= qual[i] <= 126. Prediction residues thus can range from
     * 33-126 = -93 up to 126. For all computations we can thus use (signed)
     * char's.
     * We need to take special care of empty quality score lines (only
     * containing '*'). As '*' might also be an ordinary quality score, we
     * store empty lines with decimal value -126.
     */

    size_t N = QUALCODEC_MATCH_LEN;

    if (qual[0] == '*' && qual[1] == '\0')
        goto qualcodec_empty; /* retain all empty qual lines */
    if (qualenc->blkl_n == 0)
        goto qualcodec_retain; /* first line in block */

    /* Predict the first N symbols using the first N symbols from the previous
     * line.
     */
    str_t* prev = cbufstr_top(qualenc->qual_cbuf);
    if (prev->len < N) goto qualcodec_retain;

    size_t i = 0;
    for (i = 0; i < N; i++) {
        str_append_char(qualenc->residues, (qual[i] - prev->s[i]));
    }

    /* Predict all other symbols by trying to find substrings. */
    str_t* mem = str_new();

    for (i = N; i < strlen(qual); i++) {
        /* Collect memory values */
        str_clear(mem);
        size_t j = N;
        for (j = N; j > 0; j--)
            str_append_char(mem, qual[i - j]);

        /* Search all matches of mem in the circular buffer and store the
         * corresponding predictions.
         */
        str_t* matches = str_new();
        size_t cbuf_idx = 0;
        size_t cbuf_n = qualenc->qual_cbuf->n;

        do {
            str_t* ref = cbufstr_get(qualenc->qual_cbuf, cbuf_idx++);

            int match_pos = qualcodec_match(ref, mem);
            if (match_pos >= 0)
                str_append_char(matches, ref->s[match_pos]);
        } while (cbuf_idx < cbuf_n);

        /* Compute the prediction residue and write it to the output buffer. */
        if (matches->len == 0) {
            /* Did not find any matches. Just write qual[i] to output. */
            str_append_char(qualenc->residues, qual[i]);
        } else {
            /* Compute mean prediction value */
            int pred = 0;
            size_t m = 0;
            for (m = 0; m < matches->len; m++)
                pred += (int)(matches->s[m]);
            pred = (int)pred/(int)(matches->len); /* implicitly rounding down */
            char err = qual[i] - (char)pred;
            str_append_char(qualenc->residues, err);
        }

        str_free(matches);
    }
    str_append_char(qualenc->residues, (char)QUALCODEC_LINEBREAK);

    str_free(mem);

    cbufstr_push(qualenc->qual_cbuf, qual);

    qualenc->blkl_n++;

    return;

qualcodec_empty:
    str_append_char(qualenc->residues, (char)QUALCODEC_EMPTY);
    str_append_char(qualenc->residues, (char)QUALCODEC_LINEBREAK);

    qualenc->blkl_n++;

    return;

qualcodec_retain:
    str_append_cstr(qualenc->residues, qual);
    str_append_char(qualenc->residues, (char)QUALCODEC_LINEBREAK);

    cbufstr_push(qualenc->qual_cbuf, qual);

    qualenc->blkl_n++;

    return;
}

#endif /* QUALCODEC_O1 */

#ifdef QUALCODEC_O2

static void qualenc_init(qualenc_t* qualenc)
{
    qualenc->blkl_n = 0;
}

qualenc_t* qualenc_new(void)
{
    qualenc_t* qualenc = (qualenc_t*)tsc_malloc(sizeof(qualenc_t));

    qualenc->qual_cbuf = cbufstr_new(QUALCODEC_WINDOW_SZ);
    qualenc->residues = str_new();

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
        str_free(qualenc->residues);

        cbufint64_free(qualenc->qual_cbuf_len);
        cbufint64_free(qualenc->qual_cbuf_mu);
        cbufint64_free(qualenc->qual_cbuf_var);

        free(qualenc);
        qualenc = NULL;
    } else { /* qualenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void qualenc_reset(qualenc_t* qualenc)
{
    cbufstr_clear(qualenc->qual_cbuf);
    str_clear(qualenc->residues);

    cbufint64_clear(qualenc->qual_cbuf_len);
    cbufint64_clear(qualenc->qual_cbuf_mu);
    cbufint64_clear(qualenc->qual_cbuf_var);

    qualenc_init(qualenc);
}

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

void qualenc_add_record(qualenc_t* qualenc, const char* qual)
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

    if (qual[0] != '*' && qualenc->blkl_n > 0) {
        /* QUAL exists and this is not the first line in the current block.
         * Encode it!
         */
        size_t i = 0, j = 0;

        /* Compute prediction errors for the first symbols. */
        for (i = 0; i < QUALCODEC_MEM_SZ; i++) {
            char err = q[i] - qualenc->pred[q[i] * q[i] * q[i]];
            str_append_char(qualenc->residues, err);
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
            str_append_char(qualenc->residues, err);

            /* Update tables. */
            (qualenc->freq[table_idx][(size_t)q[i]])++;

            unsigned int max = 0;
            for (j = 0; j < QUALCODEC_RANGE; j++) {
                if (qualenc->freq[table_idx][j] > max)
                    max = j;
            }
            qualenc->pred[table_idx] = max;
        }
        str_append_cstr(qualenc->residues, "\n");

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
        str_append_cstr(qualenc->residues, qual);
        str_append_cstr(qualenc->residues, "\n");

        /* If this quality line is present, add it to circular buffer. */
        if (qual[0] != '*') {
            cbufstr_push(qualenc->qual_cbuf, q);
            cbufint64_push(qualenc->qual_cbuf_len, strlen(qual));
            cbufint64_push(qualenc->qual_cbuf_mu, (int64_t)qualenc_mu(q));
            cbufint64_push(qualenc->qual_cbuf_var, (int64_t)qualenc_var(q));
        }
    }

    qualenc->blkl_n++;
}

#endif /* QUALCODEC_O2 */

size_t qualenc_write_block(qualenc_t* qualenc, FILE* ofp)
{
    size_t blk_sz = 0;

    /* Compress whole block with an entropy coder */
    unsigned char* residues = (unsigned char*)qualenc->residues->s;
    unsigned int residues_sz = (unsigned int)qualenc->residues->len;
    unsigned int data_sz = 0;
    unsigned char* data = arith_compress_o0(residues, residues_sz, &data_sz);

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

/*
 * Decoder
 * -----------------------------------------------------------------------------
 */

#ifdef QUALCODEC_O0

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

#endif /* QUALCODEC_O0 */

#ifdef QUALCODEC_O1

static void qualdec_init(qualdec_t* qualdec)
{

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
        free(qualdec);
        qualdec = NULL;
    } else { /* qualdec == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void qualdec_reset(qualdec_t* qualdec)
{
    cbufstr_clear(qualdec->qual_cbuf);
    qualdec_init(qualdec);
}

static void qualdec_decode_residues(qualdec_t*     qualdec,
                                    unsigned char* residues,
                                    size_t         residues_sz,
                                    str_t**        qual)
{
    size_t N = QUALCODEC_MATCH_LEN;

    size_t line_cnt = 0;
    size_t col_cnt = 0;
    size_t i = 0;

    while (i < residues_sz) {
        if ((char)residues[i] == (char)QUALCODEC_LINEBREAK) {
            cbufstr_push(qualdec->qual_cbuf, qual[line_cnt]->s);
            line_cnt++;
            col_cnt = 0;
            i++;
            continue;
        }

        if ((char)residues[i] == (char)QUALCODEC_EMPTY) {
            str_append_char(qual[line_cnt], '*');
            line_cnt++;
            col_cnt = 0;
            i += 2; /* skip next QUALCODEC_LINEBREAK */
            continue;
        }

        if (line_cnt == 0) {
            str_append_char(qual[line_cnt], (const char)residues[i]);
            col_cnt++;
            i++;
            continue;
        }

        /* Predict the first N symbols using the first N symbols from the
         * previous line.
         */
        if (col_cnt < N) {
            str_t* prev = cbufstr_top(qualdec->qual_cbuf);
            if (prev->len < N) {
                str_append_char(qual[line_cnt], (const char)residues[i]);
                col_cnt++;
                i++;
                continue;
            }

            str_append_char(qual[line_cnt], (char)residues[i]+prev->s[col_cnt]);

            col_cnt++;
            i++;
            continue;
        }

        /* Predict all other symbols by trying to find substrings. */

        /* Collect memory values */
        str_t* mem = str_new();
        size_t j = N;
        for (j = N; j > 0; j--)
            str_append_char(mem, qual[line_cnt]->s[col_cnt - j]);

        /* Search all matches of mem in the circular buffer and store the
         * corresponding predictions.
         */
        str_t* matches = str_new();
        size_t cbuf_idx = 0;
        size_t cbuf_n = qualdec->qual_cbuf->n;

        do {
            str_t* ref = cbufstr_get(qualdec->qual_cbuf, cbuf_idx++);

            int match_pos = qualcodec_match(ref, mem);
            if (match_pos >= 0)
                str_append_char(matches, ref->s[match_pos]);
        } while (cbuf_idx < cbuf_n);

        str_free(mem);

        /* Compute the prediction residue and write it to output buffer. */
        if (matches->len == 0) {
            /* Did not find any matches. Just write residues[i] to output. */
            str_append_char(qual[line_cnt], (const char)residues[i]);
        } else {
            /* Compute mean prediction value */
            int pred = 0;
            size_t m = 0;
            for (m = 0; m < matches->len; m++)
                pred += (int)(matches->s[m]);
            pred = (int)pred/(int)(matches->len); /* implicitly rounding down */
            char q = residues[i] + (char)pred;
            str_append_char(qual[line_cnt], q);
        }

        str_free(matches);

        col_cnt++;
        i++;
    }

    qualdec_reset(qualdec);
}

#endif /* QUALCODEC_O1 */

#ifdef QUALCODEC_O2

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

}

#endif /* QUALCODEC_O2 */

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
    unsigned char* residues = arith_decompress_o0(data, data_sz, &residues_sz);
    free(data);

    tsc_vlog("Decompressed qual block: %zu bytes -> %zu bytes (%5.2f%%)\n",
             data_sz, residues_sz, (double)residues_sz/(double)data_sz*100);

    qualdec_decode_residues(qualdec, residues, residues_sz, qual);

    tsc_vlog("Decoded qual block\n");

    free(residues); /* free memory used for decoded bitstream */

    qualdec_reset(qualdec);
}

