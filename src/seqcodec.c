/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "seqcodec.h"
/*#include "accodec.h"*/
#include "frw.h"
#include "tsclib.h"
#include <float.h>
#include <math.h>
#include <string.h>


/*****************************************************************************
 * Encoder                                                                   *
 *****************************************************************************/
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
    DEBUG("pos cigar seq: %llu %s %s", pos, cigar, seq);
}

static void seqenc_diff(str_t* seq, str_t* ref, str_t* diff)
{
    DEBUG("seq: %s", seq->s);
    DEBUG("ref: %s", ref->s);
}

static double seqenc_entropy(str_t* seq)
{
    /* Make histogram */
    unsigned int i = 0;
    unsigned int freq[256];
    memset(freq, 0x00000000, 256);
    for (i = 0; i < seq->size; i++) {
        freq[(int)(seq->s[i])]++;
    }

    /* Calculate entropy */
    double n = (double)seq->n;
    double h = 0.0;
    for (i = 0; i < 256; i++) {
        if (freq[i] != 0) {
            double prob = (double)freq[i] / n;
            h -= prob * log2(prob);
        }
    }

    return h;
}

void seqenc_add_record(seqenc_t* seqenc, uint64_t pos, const char* cigar, const char* seq)
{    
    if (cigar[0] != '*' && seqenc->block_lc > 0) {
        /* 'seq' is mapped and this is not the first record in the current
         * block. Encode it!
         */

        double entropy = 0;
        double entropy_min = DBL_MAX;

        str_t* exp = str_new();      /* expanded current sequence */
        str_t* ref = str_new();      /* expanded reference sequence */
        str_t* diff = str_new();     /* difference sequence */
        str_t* diff_min = str_new(); /* best difference sequence */

        /* Expand current sequence */
        seqenc_expand(exp, pos, cigar, seq);

        /* Push pos, cigar, seq and exp to circular buffers */
        cbufint64_push(seqenc->pos_cbuf, pos);
        cbufstr_push(seqenc->cigar_cbuf, cigar);
        cbufstr_push(seqenc->seq_cbuf, seq);
        cbufstr_push(seqenc->exp_cbuf, exp->s);

        /* Compute differences from exp to every record in the buffer */
        size_t cbuf_idx = 0;
        size_t cbuf_sz = seqenc->pos_cbuf->sz;
        size_t cbuf_idx_min = 0;

        do {
            /* Get expanded reference sequence from buffer */
            DEBUG("%d %d %d", seqenc->exp_cbuf->pos, seqenc->exp_cbuf->sz, seqenc->exp_cbuf->n);
            ref = cbufstr_get(seqenc->exp_cbuf, cbuf_idx++);

            /* Compute differences */
            seqenc_diff(diff, exp, ref);

            /* Check the entropy of the difference */
            entropy = seqenc_entropy(diff);
            if (entropy < entropy_min) {
                entropy_min = entropy;
                cbuf_idx_min = cbuf_idx;
                str_copy_str(diff_min, diff);
            }
        } while (cbuf_idx < cbuf_sz /* && entropy_curr > threshold */);

        /* Append to output stream */
        str_append_char(seqenc->out_buf, (char)((cbuf_idx_min >> 24) & 0xFF));
        str_append_char(seqenc->out_buf, (char)((cbuf_idx_min >> 16) & 0xFF));
        str_append_char(seqenc->out_buf, (char)((cbuf_idx_min >>  8) & 0xFF));
        str_append_char(seqenc->out_buf, (char)((cbuf_idx_min      ) & 0xFF));
        str_append_str(seqenc->out_buf, diff_min);

        str_free(exp);
        str_free(ref);
        str_free(diff);
        str_free(diff_min);

    } else {
        /* 'seq' is not mapped or this is the first sequence in the current
         * block.
         */
        str_append_char(seqenc->out_buf, (char)(0xFF));
        str_append_char(seqenc->out_buf, (char)(0xFF));
        str_append_char(seqenc->out_buf, (char)(0xFF));
        str_append_char(seqenc->out_buf, (char)(0xFF));
        //str_append_cstr(seqenc->out_buf, pos);
        str_append_cstr(seqenc->out_buf, cigar);
        str_append_cstr(seqenc->out_buf, seq);
    }

    seqenc->block_lc++;

    /* LEGACY - COULD BE DELETED
    if (seqenc->buf_pos < seqenc->block_sz) {
        seqenc->pos_buf[seqenc->buf_pos] = pos;
        str_copy_cstr(seqenc->cigar_buf[seqenc->buf_pos], cigar);
        str_copy_cstr(seqenc->seq_buf[seqenc->buf_pos], seq);
        seqenc->buf_pos++;
    } else {
        tsc_error("Block buffer overflow.\n");
    }
    */
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

void seqenc_write_block(seqenc_t* seqenc, FILE* ofp)
{
    tsc_log("Writing SEQ- block: %zu bytes\n", seqenc->out_buf->n);

    /*
     * Write block:
     * - 4 bytes: identifier
     * - 8 bytes: number of bytes (n)
     * - n bytes: block content
     */
    fwrite_cstr(ofp, "SEQ-");
    fwrite_uint64(ofp, (uint64_t)seqenc->out_buf->n);
    fwrite_buf(ofp, (unsigned char*)seqenc->out_buf->s, seqenc->out_buf->n);

    /* Reset encoder for next block */
    seqenc_reset(seqenc);
}

/*****************************************************************************
 * Decoder                                                                   *
 *****************************************************************************/
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
    /*
     * Read block header:
     * - 4 bytes: identifier (must equal "SEQ-")
     * - 8 bytes: number of bytes in block
     */
    unsigned char block_id[4];
    fread_buf(ifp, block_id, 4);
    uint64_t block_nb;
    fread_uint64(ifp, &block_nb);
    tsc_log("Reading SEQ- block: %zu bytes\n", block_nb);

    /* Decode block content with block_nb bytes */
    /* TODO */

    /* Reset decoder for next block */
    seqdec_reset(seqdec);
}

