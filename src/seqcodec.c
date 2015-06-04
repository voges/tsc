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
}

seqenc_t* seqenc_new(const size_t block_sz)
{
    seqenc_t* seqenc = (seqenc_t*)tsc_malloc_or_die(sizeof(seqenc_t));
    seqenc->pos_cbuf = cbufint64_new(SEQCODEC_WINDOW_SZ);
    seqenc->cigar_cbuf = cbufstr_new(SEQCODEC_WINDOW_SZ);
    seqenc->seq_cbuf = cbufstr_new(SEQCODEC_WINDOW_SZ);
    seqenc->exp_cbuf = cbufstr_new(SEQCODEC_WINDOW_SZ);
    seqenc->out = str_new();
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
        str_free(seqenc->out);
        free((void*)seqenc);
        seqenc = NULL;
    } else { /* fileenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void seqenc_expand(str_t* exp, uint64_t pos, const char* cigar, const char* seq)
{
    DEBUG("Expanding: %lu %s %s\n", pos, cigar, seq);
}

static void seqenc_diff(str_t* seq, str_t* ref, str_t* diff)
{
    DEBUG("Computing differences ...\n");
    DEBUG("seq: %s\n", seq->s);
    DEBUG("ref: %s\n", ref->s);
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
        str_append_str(seqenc->out, cbuf_idx_min);
        str_append_str(seqenc->out, diff_min);

        str_free(exp);
        str_free(ref);
        str_free(diff);
        str_free(diff_min);

    } else {
        /* 'seq' is not mapped or this is the first sequence in the current
         * block.
         */
        str_append_cstr(seqenc->out, pos);
        str_append_cstr(seqenc->out, cigar);
        str_append_cstr(seqenc->out, seq);
    }

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

void seqenc_write_block(seqenc_t* seqenc, FILE* fp)
{
    DEBUG("Writing block ...");

    /*
     * Write block:
     * - 4 bytes: identifier
     * - 8 bytes: number of bytes (n)
     * - n bytes: block content
     */
    fwrite_cstr(fp, "SEQ");
    fwrite_byte(fp, 0x00);
    fwrite_uint32(fp, seqenc->out->n);
    fwrite_buf(fp, (unsigned char*)seqenc->out->s, seqenc->out->n);
    /*
    unsigned int i = 0;
    for (i = 0; i < seqenc->buf_pos; i++) {
        fwrite_uint64(fp, seqenc->pos_buf[i]);
        fwrite_cstr(fp, "\t");
        fwrite_cstr(fp, seqenc->cigar_buf[i]->s);
        fwrite_cstr(fp, "\t");
        fwrite_cstr(fp, seqenc->seq_buf[i]->s);
        fwrite_cstr(fp, "\n");
    }
    */

    /* Reset encoder */
    //seqenc_init(seqenc);
}

/*****************************************************************************
 * Decoder                                                                   *
 *****************************************************************************/
static void seqdec_init(seqdec_t* seqdec)
{

}

seqdec_t* seqdec_new(void)
{
    seqdec_t* seqdec = (seqdec_t*)tsc_malloc_or_die(sizeof(seqdec_t));

    seqdec_init(seqdec);
    return seqdec;
}

void seqdec_free(seqdec_t* seqdec)
{
    if (seqdec != NULL) {

        free((void*)seqdec);
        seqdec = NULL;
    } else { /* seqdec == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.\n");
    }
}

void seqdec_decode_block(seqdec_t* seqenc, FILE* fp)
{

}

