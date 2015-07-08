/*
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>
 *
 * This file is part of tsc.
 */

/*
 * Nuc o0 encoder: Fall-through to entropy coder
 *
 * Nuc o0 block format:
 *   unsigned char  blk_id[8] : "nuc----" + '\0'
 *   uint64_t       blkl_n    : no. of lines in block
 *   uint64_t       data_sz   : data size
 *   uint64_t       data_crc  : CRC64 of compressed data
 *   unsigned char* data      : data
 */

/*
 * Nuc o1 encoder: Differential coding with short-time memory reference
 * (patent pending)
 *
 * Nuc o1 block format:
 *   unsigned char  blk_id[8]               : "nuc----" + '\0'
 *   uint64_t       blkl_n                  : no. of lines in block
 *   uint64_t       pos_residues_data_sz    : position data size
 *   uint64_t       pos_residues_data_crc   : CRC64 of compressed position data
 *   unsigned char* pos_residues_data       : position data
 *   uint64_t       cigar_residues_data_sz  : ...
 *   uint64_t       cigar_residues_data_crc : ...
 *   unsigned char* cigar_residues_data     : ...
 *   uint64_t       seq_residues_data_sz    : ...
 *   uint64_t       seq_residues_data_crc   : ...
 *   unsigned char* seq_residues_data       : ...
 */

#include "nuccodec.h"

#ifdef NUCCODEC_O0

#include "../arithcodec/arithcodec.h"
#include "../crc64/crc64.h"
#include "../frw/frw.h"
#include "../frw/frw.h"
#include "../tsclib.h"
#include <inttypes.h>
#include <string.h>

#endif /* NUCCODEC_O0 */

#ifdef NUCCODEC_O1

#include "../arithcodec/arithcodec.h"
#include "../crc64/crc64.h"
#include "../frw/frw.h"
#include "../frw/frw.h"
#include "../tsclib.h"
#include <ctype.h>
#include <float.h>
//#include <inttypes.h>
#include <math.h>
#include <string.h>

#define NUCCODEC_EMPTY -126
#define NUCCODEC_LINEBREAK -125
#define NUCCODEC_PLAIN -124

#endif /* NUCCODEC_O1 */

/*
 * Encoder
 * -----------------------------------------------------------------------------
 */

#ifdef NUCCODEC_O0

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
    str_clear(nucenc->residues);
    nucenc_init(nucenc);
}

void nucenc_add_record(nucenc_t*      nucenc,
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

#endif /* NUCCODEC_O0 */

#ifdef NUCCODEC_O1

static void nucenc_init(nucenc_t* nucenc)
{
    nucenc->blkl_n = 0;
    nucenc->first = false;
}

nucenc_t* nucenc_new(void)
{
    nucenc_t* nucenc = (nucenc_t*)tsc_malloc(sizeof(nucenc_t));

    nucenc->pos_cbuf = cbufint64_new(NUCCODEC_WINDOW_SZ);
    nucenc->cigar_cbuf = cbufstr_new(NUCCODEC_WINDOW_SZ);
    nucenc->exp_cbuf = cbufstr_new(NUCCODEC_WINDOW_SZ);

    nucenc->pos_residues = bbuf_new();
    nucenc->cigar_residues = str_new();
    nucenc->seq_residues = str_new();

    nucenc_init(nucenc);

    return nucenc;
}

void nucenc_free(nucenc_t* nucenc)
{
    if (nucenc != NULL) {
        cbufint64_free(nucenc->pos_cbuf);
        cbufstr_free(nucenc->cigar_cbuf);
        cbufstr_free(nucenc->exp_cbuf);

        bbuf_free(nucenc->pos_residues);
        str_free(nucenc->cigar_residues);
        str_free(nucenc->seq_residues);

        free(nucenc);
        nucenc = NULL;
    } else { /* nucenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void nucenc_reset(nucenc_t* nucenc)
{
    cbufint64_clear(nucenc->pos_cbuf);
    cbufstr_clear(nucenc->cigar_cbuf);
    cbufstr_clear(nucenc->exp_cbuf);

    bbuf_clear(nucenc->pos_residues);
    str_clear(nucenc->cigar_residues);
    str_clear(nucenc->seq_residues);

    nucenc_init(nucenc);
}

static void nucenc_expand(str_t* exp, uint64_t pos, const char* cigar,
                          const char* seq)
{
    /* TODO: check this :) */
    //DEBUG("pos: %"PRIu64" cigar: %s", pos, cigar);
    //DEBUG("seq: \t%s", seq);
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
        case 'P': {
            size_t i = 0;
            for (i = 0; i < op_len; i++) { str_append_char(exp, '-'); }
            break;
        }
        default: tsc_error("Bad CIGAR string: %s\n", cigar);
        }

        op_len = 0;
    }

    //DEBUG("exp: \t%s", exp->s);
}

static bool nucenc_diff(str_t* diff, str_t* exp, uint64_t exp_pos, str_t* ref,
                        uint64_t ref_pos)
{
    int64_t pos_off = exp_pos - ref_pos; /* position offset */
    if (pos_off < 0)
        return false;

    if (pos_off > ref->len) /* position offset too large */
        return false;

    uint64_t match_len = ref->len - pos_off; /* length of match */
    if ((exp->len + pos_off) < match_len)
        match_len = exp->len + pos_off;

    str_clear(diff);
    str_reserve(diff, exp->len);

    /* Compute differences from exp to ref */
    size_t ref_idx = pos_off;
    size_t exp_idx = 0;
    int d = 0;
    size_t i = 0;

    for (i = pos_off; i < match_len; i++) {
        d = (int)exp->s[exp_idx++] - (int)ref->s[ref_idx++] + ((int)'T');
        str_append_char(diff, (char)d);
    }

    while (i < exp->len)
        str_append_char(diff, exp->s[i++]);

    /*printf("pos_off: %"PRIu64"\n", pos_off);
    printf("ref:  %s\n", ref->s);
    printf("exp:  ");
    int k = 0; for (k = 0; k < pos_off; k++) printf(" ");
    printf("%s\n", exp->s);
    printf("diff: ");
    for (k = 0; k < pos_off; k++) printf(" ");
    printf("%s\n", diff->s);*/

    return true;
}

static double nucenc_entropy(str_t* seq)
{
    /* Make histogram */
    unsigned int i = 0;
    unsigned int freq[256];
    memset(freq, 0, sizeof(freq));
    for (i = 0; i < seq->len; i++)
        freq[(int)(seq->s[i])]++;

    /* Calculate entropy */
    double n = (double)seq->len;
    double h = 0.0;
    for (i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            double prob = (double)freq[i] / n;
            h -= prob * log2(prob);
        }
    }

    return h;
}

void nucenc_add_record(nucenc_t*      nucenc,
                       const int64_t  pos,
                       const char*    cigar,
                       const char*    seq)
{
    /* We assume five possible symbols for seq:
     *   A=65, C=67, G=71, N=78, T=84 or a=97, c=99, g=103, n=110, t=116
     * Prediction residues thus can range from 65-116 = -51 up to 116. For all
     * computations we can thus use (signed) char's.
     * We have to take care of two special symbols: '*' (indicating an 'empty'
     * sequence or CIGAR string) and '\n' (line break).
     * Empty lines are stored as decimal value -126 and line breaks as -125.
     * If the prediction algorithm failed, decimal value -124 is prepended
     * to the sequence output string.
     */

    /* TODO: check, if seq could be present if cigar='*', or vice versa */
    if ((cigar[0] == '*' && cigar[1] == '\0')
        || (seq[0] == '*' && seq[1] == '\0')) {
        /* TODO: mark pos as 'empty' */
        bbuf_append_uint64(nucenc->pos_residues, (uint64_t)pos);
        str_append_char(nucenc->cigar_residues, (char)NUCCODEC_EMPTY);
        str_append_char(nucenc->seq_residues, (char)NUCCODEC_EMPTY);
        nucenc->blkl_n++;
        return;
    }

    if (!nucenc->first) {
        nucenc->first = true;

        bbuf_append_uint64(nucenc->pos_residues, (uint64_t)pos);
        str_append_cstr(nucenc->cigar_residues, cigar);
        str_append_char(nucenc->cigar_residues, (char)NUCCODEC_LINEBREAK);
        str_append_cstr(nucenc->seq_residues, seq);
        str_append_char(nucenc->seq_residues, (char)NUCCODEC_LINEBREAK);

        cbufint64_push(nucenc->pos_cbuf, pos);
        cbufstr_push(nucenc->cigar_cbuf, cigar); /* TODO: necessary? */
        str_t* exp = str_new();
        nucenc_expand(exp, pos, cigar, seq);
        cbufstr_push(nucenc->exp_cbuf, exp->s);
        str_free(exp);

        nucenc->blkl_n++;

        return;
    }

    /*
     * Predict the current sequence with the custom algorithm.
     */

    str_t* exp = str_new();       /* expanded current sequence */
    str_t* diff = str_new();      /* difference sequence */
    str_t* diff_best = str_new(); /* best difference sequence */

    double entropy = 0.0;
    double entropy_best = DBL_MAX;

    nucenc_expand(exp, pos, cigar, seq);

    /* Compute differences from exp to every record in the circular buffer */
    size_t cbuf_idx = 0;
    size_t cbuf_n = nucenc->pos_cbuf->n;
    size_t cbuf_idx_best = 0;
    bool diff_failed = false;

    do {
        str_t* exp_ref = cbufstr_get(nucenc->exp_cbuf, cbuf_idx);
        uint64_t pos_ref = cbufint64_get(nucenc->pos_cbuf, cbuf_idx);

        /* Compute difference */
        if (!nucenc_diff(diff, exp, pos, exp_ref, pos_ref)) {
            diff_failed = true;
        } else {
            entropy = nucenc_entropy(diff);
            if (entropy < entropy_best) {
                entropy_best = entropy;
                cbuf_idx_best = cbuf_idx;
                str_copy_str(diff_best, diff);
            }
        }

        cbuf_idx++;
    } while (cbuf_idx < cbuf_n /* && entropy_curr > threshold */);

    if (diff_failed) {
        bbuf_append_uint64(nucenc->pos_residues, (uint64_t)pos);
        str_append_cstr(nucenc->cigar_residues, cigar);
        str_append_char(nucenc->cigar_residues, (char)NUCCODEC_LINEBREAK);
        str_append_char(nucenc->seq_residues, (char)NUCCODEC_PLAIN);
        str_append_cstr(nucenc->seq_residues, seq);
        str_append_char(nucenc->seq_residues, (char)NUCCODEC_LINEBREAK);

        nucenc->blkl_n++;
        return;
    }

    /* Write pos, cigar, diff_best, and cbuf_idx_min to output */

    uint64_t pos_prev = (uint64_t)cbufint64_top(nucenc->pos_cbuf);
    bbuf_append_uint64(nucenc->pos_residues, (uint64_t)pos - pos_prev);
    bbuf_append_uint64(nucenc->pos_residues, (uint64_t)cbuf_idx_best);

    /* TODO: This could be part of expand() & diff() */
    str_t* cigar_prev = cbufstr_top(nucenc->cigar_cbuf);
    str_t* cigar_res = str_new();
    str_append_cstr(cigar_res, cigar);
    if (cigar_prev->len == strlen(cigar)) {
        str_clear(cigar_res);
        size_t i = 0;
        for (i = 0; i < cigar_prev->len; i++)
            str_append_char(cigar_res, (cigar[i] - cigar_prev->s[i]));
    }
    str_append_str(nucenc->cigar_residues, cigar_res);
    str_append_char(nucenc->cigar_residues, (char)NUCCODEC_LINEBREAK);
    str_free(cigar_res);

    str_append_str(nucenc->seq_residues, diff_best);
    str_append_char(nucenc->seq_residues, (char)NUCCODEC_LINEBREAK);

    /* Add everything to cbuf's */
    cbufint64_push(nucenc->pos_cbuf, pos);
    cbufstr_push(nucenc->cigar_cbuf, cigar);
    cbufstr_push(nucenc->exp_cbuf, exp->s);

    str_free(exp);
    str_free(diff);
    str_free(diff_best);

    nucenc->blkl_n++;

    return;
}

size_t nucenc_write_block(nucenc_t* nucenc, FILE* ofp)
{
    size_t blk_sz = 0;

    /* Compress whole blocks with an entropy coder */
    unsigned char* pos_residues
                            = (unsigned char*)nucenc->pos_residues->bytes;
    unsigned int pos_residues_sz = (unsigned int)nucenc->pos_residues->sz;
    unsigned int pos_residues_data_sz = 0;
    unsigned char* pos_residues_data
                        = arith_compress_o0(pos_residues, pos_residues_sz,
                                            &pos_residues_data_sz);

    tsc_vlog("Compressed nuc-pos block: %zu bytes -> %zu bytes (%6.2f%%)\n",
             pos_residues_sz, pos_residues_data_sz,
             (double)pos_residues_data_sz / (double)pos_residues_sz*100);

    unsigned char* cigar_residues
                            = (unsigned char*)nucenc->cigar_residues->s;
    unsigned int cigar_residues_sz = (unsigned int)nucenc->cigar_residues->len;
    unsigned int cigar_residues_data_sz = 0;
    unsigned char* cigar_residues_data
                        = arith_compress_o0(cigar_residues, cigar_residues_sz,
                                            &cigar_residues_data_sz);

    tsc_vlog("Compressed nuc-cigar block: %zu bytes -> %zu bytes (%6.2f%%)\n",
             cigar_residues_sz, cigar_residues_data_sz,
             (double)cigar_residues_data_sz / (double)cigar_residues_sz*100);

    unsigned char* seq_residues
                            = (unsigned char*)nucenc->seq_residues->s;
    unsigned int seq_residues_sz = (unsigned int)nucenc->seq_residues->len;
    unsigned int seq_residues_data_sz = 0;
    unsigned char* seq_residues_data
                        = arith_compress_o0(seq_residues, seq_residues_sz,
                                            &seq_residues_data_sz);

    tsc_vlog("Compressed nuc-seq block: %zu bytes -> %zu bytes (%6.2f%%)\n",
             seq_residues_sz, seq_residues_data_sz,
             (double)seq_residues_data_sz / (double)seq_residues_sz*100);

    /* Compute CRC64 */
    uint64_t pos_residues_data_crc
                        = crc64(pos_residues_data, pos_residues_data_sz);
    uint64_t cigar_residues_data_crc
                        = crc64(cigar_residues_data, cigar_residues_data_sz);
    uint64_t seq_residues_data_crc
                        = crc64(seq_residues_data, seq_residues_data_sz);

    /* Write compressed block */
    unsigned char blk_id[8] = "nuc----"; blk_id[7] = '\0';
    blk_sz += fwrite_buf(ofp, blk_id, sizeof(blk_id));
    blk_sz += fwrite_uint64(ofp, (uint64_t)nucenc->blkl_n);

    blk_sz += fwrite_uint64(ofp, (uint64_t)pos_residues_data_sz);
    blk_sz += fwrite_uint64(ofp, (uint64_t)pos_residues_data_crc);
    blk_sz += fwrite_buf(ofp, pos_residues_data, pos_residues_data_sz);

    blk_sz += fwrite_uint64(ofp, (uint64_t)cigar_residues_data_sz);
    blk_sz += fwrite_uint64(ofp, (uint64_t)cigar_residues_data_crc);
    blk_sz += fwrite_buf(ofp, cigar_residues_data, cigar_residues_data_sz);

    blk_sz += fwrite_uint64(ofp, (uint64_t)seq_residues_data_sz);
    blk_sz += fwrite_uint64(ofp, (uint64_t)seq_residues_data_crc);
    blk_sz += fwrite_buf(ofp, seq_residues_data, seq_residues_data_sz);

    tsc_vlog("Wrote nuc block: %zu lines, %zu data bytes\n",
             nucenc->blkl_n,
             pos_residues_data_sz
             + cigar_residues_data_sz
             + seq_residues_data_sz);

    nucenc_reset(nucenc); /* reset encoder for next block */
    free(pos_residues_data); /* free memory used for encoded bitstream */
    free(cigar_residues_data);
    free(seq_residues_data);

    return blk_sz;
}

#endif /* NUCCODEC_O1 */

/*
 * Decoder
 * -----------------------------------------------------------------------------
 */

#ifdef NUCCODEC_O0

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

static void nucdec_decode_residues(nucdec_t*      nucdec,
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

    /* Read and check block, then decompress it and decode the prediction
     * residues.
     */
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

    nucdec_decode_residues(nucdec, residues, residues_sz, pos, cigar, seq);

    tsc_vlog("Decoded nuc block\n");

    free(residues); /* free memory used for decoded bitstream */

    nucdec_reset(nucdec);
}

#endif /* NUCCODEC_O0 */

#ifdef NUCCODEC_O1

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

static void nucdec_decode_residues(nucdec_t*      nucdec,
                                   unsigned char* residues,
                                   size_t         residues_sz,
                                   int64_t*       pos,
                                   str_t**        cigar,
                                   str_t**        seq)
{

}

void nucdec_decode_block(nucdec_t* nucdec,
                         FILE*     ifp,
                         int64_t*  pos,
                         str_t**   cigar,
                         str_t**   seq)
{

}

#endif /* NUCCODEC_O1 */

