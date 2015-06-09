/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "seqcodec.h"
#include "accodec.h"
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
static void seqenc_init(seqenc_t* seqenc, const size_t block_sz)
{
    seqenc->block_sz = block_sz;
    seqenc->block_lc = 0;
}

seqenc_t* seqenc_new(const size_t block_sz)
{
    seqenc_t* seqenc = (seqenc_t*)tsc_malloc_or_die(sizeof(seqenc_t));
    seqenc->pos_cbuf = cbufint64_new(SEQCODEC_WINDOW_SZ);
    seqenc->cigar_cbuf = cbufstr_new(SEQCODEC_WINDOW_SZ);
    seqenc->seq_cbuf = cbufstr_new(SEQCODEC_WINDOW_SZ);
    seqenc->exp_cbuf = cbufstr_new(SEQCODEC_WINDOW_SZ);
    seqenc->out_buf = str_new();
    seqenc_init(seqenc, block_sz);
    return seqenc;
}

void seqenc_free(seqenc_t* seqenc)
{
    if (seqenc != NULL) {
        cbufint64_free(seqenc->pos_cbuf);
        cbufstr_free(seqenc->cigar_cbuf);
        cbufstr_free(seqenc->seq_cbuf);
        cbufstr_free(seqenc->exp_cbuf);
        str_free(seqenc->out_buf);
        free((void*)seqenc);
        seqenc = NULL;
    } else { /* seqenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void seqenc_expand(str_t* exp, uint64_t pos, const char* cigar, const char* seq)
{
    str_clear(exp);

    size_t cigar_idx = 0;
    size_t cigar_len = strlen(cigar);
    size_t op_len = 0; /* length of current CIGAR operation */
    size_t seq_idx = 0;

    /* Iterate through CIGAR string and expand SEQ. */
    for (cigar_idx = 0; cigar_idx < cigar_len; cigar_idx++) {
        if (isdigit(cigar[cigar_idx])) {
            op_len = op_len * 10 + cigar[cigar_idx] - '0';
            continue;
        }

        switch (cigar[cigar_idx]) {
        case 'M':
        case '=':
        case 'X':
            str_append_cstrn(exp, &seq[seq_idx], op_len);
            seq_idx += op_len;
            break;
        case 'I':
        case 'S':
            str_append_cstrn(exp, &seq[seq_idx], op_len);
            seq_idx += op_len;
            break;
        case 'D':
        case 'N': {
            size_t i = 0;
            for (i = 0; i < op_len; i++) { str_append_char(exp, '_'); }
            break;
        }
        case 'H':
        case 'P':
            str_append_cstrn(exp, &seq[seq_idx], op_len);
            seq_idx += op_len;
            break;
        default: tsc_error("Bad CIGAR string: %s\n", cigar);
        }

        op_len = 0;
    }
    DEBUG("exp: %s", exp->s);
}

static void seqenc_diff(str_t* diff, str_t* exp, uint64_t exp_pos, str_t* ref, uint64_t ref_pos)
{
    /* Compute position offset. */
    uint64_t pos_off = exp_pos - ref_pos;

    /* Determine length of match. */
    if (pos_off > ref->n) tsc_error("Position offset too large: %d\n", pos_off);
    uint64_t match_len = ref->n - pos_off;
    if (exp->n + pos_off < match_len) match_len = exp->n + pos_off;
    
    /* Allocate memory for diff. */
    str_clear(diff);
    str_reserve(diff, exp->n);
    
    /* Compute differences from exp to ref. */
    size_t ref_idx = pos_off;
    size_t exp_idx = 0;
    int d = 0;
    size_t i = 0;
    for (i = pos_off; i < match_len; i++) {
        d = (int)exp->s[exp_idx++] - (int)ref->s[ref_idx++] + ((int)'T');
        str_append_char(diff, (char)d);
    }
    while (i < exp->n) str_append_char(diff, exp->s[i++]);

    DEBUG("pos_off: %"PRIu64"", pos_off);
    DEBUG("ref:  %s", ref->s);
    DEBUG("exp:  %s", exp->s);
    DEBUG("diff: %s", diff->s);
}

static double seqenc_entropy(str_t* seq)
{
    /* Make histogram. */
    unsigned int i = 0;
    unsigned int freq[256];
    memset(freq, 0, sizeof(freq));
    for (i = 0; i < seq->n; i++) freq[(int)(seq->s[i])]++;

    /* Calculate entropy. */
    double n = (double)seq->n;
    double h = 0.0;
    for (i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            double prob = (double)freq[i] / n;
            h -= prob * log2(prob);
        }
    }

    return h;
}

void seqenc_add_record(seqenc_t* seqenc, uint64_t pos, const char* cigar, const char* seq)
{    
    /* Allocate memory for encoding. */
    str_t* exp = str_new();      /* expanded current sequence */
    str_t* diff = str_new();     /* difference sequence */
    str_t* diff_min = str_new(); /* best difference sequence */

    if (cigar[0] != '*' && seq[0] != '*' && seqenc->block_lc > 0) {
        /* SEQ is mapped , SEQ & CIGAR are present and this is not the first
         * record in the current block. Encode it!
         */

        double entropy = 0;
        double entropy_min = DBL_MAX;

        /* Expand current record. */
        seqenc_expand(exp, pos, cigar, seq);

        /* Compute differences from EXP to every record in the buffer. */
        size_t cbuf_idx = 0;
        size_t cbuf_n = seqenc->pos_cbuf->n;
        //size_t cbuf_idx_min = 0;

        do {
            /* Get expanded reference sequence from buffer. */
            str_t* ref = cbufstr_get(seqenc->exp_cbuf, cbuf_idx);
            uint64_t ref_pos = cbufint64_get(seqenc->pos_cbuf, cbuf_idx);
            cbuf_idx++;

            /* Compute difference.s */
            seqenc_diff(diff, exp, pos, ref, ref_pos);

            /* Check the entropy of the difference. */
            entropy = seqenc_entropy(diff);
            if (entropy < entropy_min) {
                entropy_min = entropy;
                //cbuf_idx_min = cbuf_idx;
                str_copy_str(diff_min, diff);
            }
        } while (cbuf_idx < cbuf_n /* && entropy_curr > threshold */);

        /* Append to output stream. */
        /* TODO: Append cbuf_idx_min */
        str_append_str(seqenc->out_buf, diff_min);
        str_append_cstr(seqenc->out_buf, "\n");

        /* Push POS, CIGAR, SEQ and EXP to circular buffers. */
        cbufint64_push(seqenc->pos_cbuf, pos);
        cbufstr_push(seqenc->cigar_cbuf, cigar);
        cbufstr_push(seqenc->seq_cbuf, seq);
        cbufstr_push(seqenc->exp_cbuf, exp->s);

        str_clear(exp);
        str_clear(diff);
        str_clear(diff_min);

    } else {
        /* SEQ is not mapped or is not present, or CIGAR is not present or
         * this is the first sequence in the current block.
         */

        /* Output record. */
        if (pos > pow(10, 10) - 1) tsc_error("Buffer too small for POS data!\n");
        char tmp[101]; snprintf(tmp, sizeof(tmp), "%jd", pos);
        str_append_cstr(seqenc->out_buf, tmp);
        str_append_cstr(seqenc->out_buf, "\t");
        str_append_cstr(seqenc->out_buf, cigar);
        str_append_cstr(seqenc->out_buf, "\t");
        str_append_cstr(seqenc->out_buf, seq);
        str_append_cstr(seqenc->out_buf, "\n");

        /* If this record is present, add it to circular buffers. */
        if (cigar[0] != '*' && seq[0] != '*') {
            /* Expand current record. */
            seqenc_expand(exp, pos, cigar, seq);

            /* Push POS, CIGAR, SEQ and EXP to circular buffers. */
            cbufint64_push(seqenc->pos_cbuf, pos);
            cbufstr_push(seqenc->cigar_cbuf, cigar);
            cbufstr_push(seqenc->seq_cbuf, seq);
            cbufstr_push(seqenc->exp_cbuf, exp->s);
        }
    }

    str_free(exp);
    str_free(diff);
    str_free(diff_min);

    /*DEBUG("out_buf: \n%s", seqenc->out_buf->s);*/
    seqenc->block_lc++;
}

static void seqenc_reset(seqenc_t* seqenc)
{
    seqenc->block_sz = 0;
    seqenc->block_lc = 0;
    cbufint64_clear(seqenc->pos_cbuf);
    cbufstr_clear(seqenc->cigar_cbuf);
    cbufstr_clear(seqenc->seq_cbuf);
    cbufstr_clear(seqenc->exp_cbuf);
    str_clear(seqenc->out_buf);
}

size_t seqenc_write_block(seqenc_t* seqenc, FILE* ofp)
{
    /* Write block:
     * - 4 bytes: identifier
     * - 4 bytes: number of bytes (n)
     * - n bytes: block content
     */
    size_t nbytes = 0;
    nbytes += fwrite_cstr(ofp, "SEQ-");

    //nbytes += fwrite_uint64(ofp, (uint64_t)seqenc->out_buf->n);
    //nbytes += fwrite_buf(ofp, (unsigned char*)seqenc->out_buf->s, seqenc->out_buf->n);

    unsigned char* ac_in = (unsigned char*)seqenc->out_buf->s;
    unsigned int ac_in_sz = seqenc->out_buf->n;
    unsigned int ac_out_sz = 0;
    unsigned char* ac_out = arith_compress_O0(ac_in, ac_in_sz, &ac_out_sz);

    tsc_log("Writing SEQ- block: %zu bytes\n", ac_out_sz);

    nbytes += fwrite_uint32(ofp, ac_out_sz);
    nbytes += fwrite_buf(ofp, ac_out, ac_out_sz);
    free((void*)ac_out);

    /* Reset encoder for next block. */
    seqenc_reset(seqenc);
    
    return nbytes;
}

/******************************************************************************
 * Decoder                                                                    *
 ******************************************************************************/
static void seqdec_init(seqdec_t* seqdec)
{
    seqdec->block_sz = 0;
    seqdec->block_lc = 0;
}

seqdec_t* seqdec_new(void)
{
    seqdec_t* seqdec = (seqdec_t*)tsc_malloc_or_die(sizeof(seqdec_t));
    seqdec->pos_cbuf = cbufint64_new(SEQCODEC_WINDOW_SZ);
    seqdec->cigar_cbuf = cbufstr_new(SEQCODEC_WINDOW_SZ);
    seqdec->seq_cbuf = cbufstr_new(SEQCODEC_WINDOW_SZ);
    seqdec->exp_cbuf = cbufstr_new(SEQCODEC_WINDOW_SZ);
    seqdec_init(seqdec);
    return seqdec;
}

void seqdec_free(seqdec_t* seqdec)
{
    if (seqdec != NULL) {
        cbufint64_free(seqdec->pos_cbuf);
        cbufstr_free(seqdec->cigar_cbuf);
        cbufstr_free(seqdec->seq_cbuf);
        cbufstr_free(seqdec->exp_cbuf);
        free((void*)seqdec);
        seqdec = NULL;
    } else { /* seqdec == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void seqdec_reset(seqdec_t* seqdec)
{
    seqdec->block_sz = 0;
    seqdec->block_lc = 0;
    cbufint64_clear(seqdec->pos_cbuf);
    cbufstr_clear(seqdec->cigar_cbuf);
    cbufstr_clear(seqdec->seq_cbuf);
    cbufstr_clear(seqdec->exp_cbuf);
}

void seqdec_decode_block(seqdec_t* seqdec, FILE* ifp, str_t** seq)
{
    /* Read block header:
     * - 4 bytes: identifier (must equal "SEQ-")
     * - 4 bytes: number of bytes in block
     */
    unsigned char block_id[4];
    uint32_t block_sz;

    if (fread_buf(ifp, block_id, 4) != sizeof(block_id))
        tsc_error("Could not read block header!\n");
    if (fread_uint32(ifp, &block_sz) != sizeof(block_sz))
        tsc_error("Could not read number of bytes in block!\n");
    if (strncmp("SEQ-", (const char*)block_id, 4))
        tsc_error("Wrong block id: %s\n", block_id);
    tsc_log("Reading %s block: %zu bytes\n", block_id, block_sz);

    /* Decode block content with block_nb bytes. */
    /* TODO */
    bbuf_t* buf = bbuf_new();
    bbuf_reserve(buf, block_sz);
    fread_buf(ifp, buf->bytes, block_sz);
    bbuf_free(buf);

    /* Reset decoder for next block. */
    seqdec_reset(seqdec);
}

