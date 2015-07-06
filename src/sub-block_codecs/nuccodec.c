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
 *   unsigned char  blk_id[8] : "nuc----" + '\0'
 *   uint64_t       blkl_n    : no. of lines in block
 *   uint64_t       data_sz   : data size
 *   uint64_t       data_crc  : CRC64 of compressed data
 *   unsigned char* data      : data
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
#include "bbuf.h"
#include "../crc64/crc64.h"
#include "../frw/frw.h"
#include "../frw/frw.h"
#include <ctype.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

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
    nucenc->diff_fail_n = 0;
}

nucenc_t* nucenc_new(void)
{
    nucenc_t* nucenc = (nucenc_t*)tsc_malloc(sizeof(nucenc_t));

    nucenc->residues = str_new();

    nucenc->pos_cbuf = cbufint64_new(NUCCODEC_WINDOW_SZ);
    nucenc->cigar_cbuf = cbufstr_new(NUCCODEC_WINDOW_SZ);
    nucenc->seq_cbuf = cbufstr_new(NUCCODEC_WINDOW_SZ);
    nucenc->exp_cbuf = cbufstr_new(NUCCODEC_WINDOW_SZ);

    nucenc->pos_res = bbuf_new();
    nucenc->cigar_res = str_new();
    nucenc->seq_res = str_new();

    nucenc_init(nucenc);

    return nucenc;
}

void nucenc_free(nucenc_t* nucenc)
{
    if (nucenc != NULL) {
        str_free(nucenc->residues);

        cbufint64_free(nucenc->pos_cbuf);
        cbufstr_free(nucenc->cigar_cbuf);
        cbufstr_free(nucenc->seq_cbuf);
        cbufstr_free(nucenc->exp_cbuf);

        bbuf_free(nucenc->pos_res);
        str_free(nucenc->cigar_res);
        str_free(nucenc->seq_res);

        free(nucenc);
        nucenc = NULL;
    } else { /* nucenc == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

static void nucenc_reset(nucenc_t* nucenc)
{
    str_clear(nucenc->residues);

    cbufint64_clear(nucenc->pos_cbuf);
    cbufstr_clear(nucenc->cigar_cbuf);
    cbufstr_clear(nucenc->seq_cbuf);
    cbufstr_clear(nucenc->exp_cbuf);

    bbuf_clear(nucenc->pos_res);
    str_clear(nucenc->cigar_res);
    str_clear(nucenc->seq_res);

    nucenc_init(nucenc);
}

static void nucenc_expand(str_t* exp, uint64_t pos, const char* cigar, const char* seq)
{
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

static bool nucenc_diff(str_t* diff, str_t* exp, uint64_t exp_pos, str_t* ref, uint64_t ref_pos)
{
    /* Compute position offset. */
    uint64_t pos_off = exp_pos - ref_pos;

    /* Determine length of match. */
    if (pos_off > ref->len) {
        //tsc_warning("Position offset too large: %d\n", pos_off);
        return false;
    }
    uint64_t match_len = ref->len - pos_off;
    if (exp->len + pos_off < match_len) match_len = exp->len + pos_off;

    /* Allocate memory for diff. */
    str_clear(diff);
    str_reserve(diff, exp->len);

    /* Compute differences from exp to ref. */
    size_t ref_idx = pos_off;
    size_t exp_idx = 0;
    int d = 0;
    size_t i = 0;
    for (i = pos_off; i < match_len; i++) {
        d = (int)exp->s[exp_idx++] - (int)ref->s[ref_idx++] + ((int)'T');
        str_append_char(diff, (char)d);
    }
    while (i < exp->len) str_append_char(diff, exp->s[i++]);

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
    /* Make histogram. */
    unsigned int i = 0;
    unsigned int freq[256];
    memset(freq, 0, sizeof(freq));
    for (i = 0; i < seq->len; i++) freq[(int)(seq->s[i])]++;

    /* Calculate entropy. */
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

static void nucenc_add_record(nucenc_t*      nucenc,
                              const int64_t  pos,
                              const char*    cigar,
                              const char*    seq)
{
    /* Allocate memory for encoding. */
    str_t* exp = str_new();      /* expanded current sequence */
    str_t* diff = str_new();     /* difference sequence */
    str_t* diff_min = str_new(); /* best difference sequence */

    if (cigar[0] != '*' && seq[0] != '*' && nucenc->blkl_n > 0) {
        /* SEQ is mapped , SEQ & CIGAR are present and this is not the first
         * record in the current block. Encode it!
         */

        bool diff_ok = false;
        double entropy = 0;
        double entropy_min = DBL_MAX;

        /* Expand current record. */
        nucenc_expand(exp, pos, cigar, seq);

        /* Compute differences from EXP to every record in the buffer. */
        size_t cbuf_idx = 0;
        size_t cbuf_n = nucenc->pos_cbuf->n;
        //size_t cbuf_idx_min = 0;

        do {
            /* Get expanded reference sequence from buffer. */
            str_t* ref = cbufstr_get(nucenc->exp_cbuf, cbuf_idx);
            uint64_t ref_pos = cbufint64_get(nucenc->pos_cbuf, cbuf_idx);
            cbuf_idx++;

            /* Compute difference */
            if (!nucenc_diff(diff, exp, pos, ref, ref_pos)) {
                nucenc->block_ln++;
                continue;
            }
            else diff_ok = true;

            /* Check the entropy of the difference. */
            if (diff_ok) {
                entropy = nucenc_entropy(diff);
                if (entropy < entropy_min) {
                    entropy_min = entropy;
                    //cbuf_idx_min = cbuf_idx;
                    str_copy_str(diff_min, diff);
                }
            }
        } while (cbuf_idx < cbuf_n /* && entropy_curr > threshold */);

        /* Append to output stream. */
        /* TODO: Append cbuf_idx_min */
        if (diff_ok) {
            str_append_str(nucenc->residues, diff_min);
        } else {
            if (pos > pow(10, 10) - 1) tsc_error("Buffer too small for POS data!\n");
            char tmp[101]; snprintf(tmp, sizeof(tmp), "%"PRIu64, pos);
            str_append_cstr(nucenc->residues, tmp);
            str_append_cstr(nucenc->residues, "\t");
            str_append_cstr(nucenc->residues, cigar);
            str_append_cstr(nucenc->residues, "\t");
            str_append_cstr(nucenc->residues, seq);
        }
        str_append_cstr(nucenc->residues, "\n");

        /* Push POS, CIGAR, SEQ and EXP to circular buffers. */
        cbufint64_push(nucenc->pos_cbuf, pos);
        cbufstr_push(nucenc->cigar_cbuf, cigar);
        cbufstr_push(nucenc->seq_cbuf, seq);
        cbufstr_push(nucenc->exp_cbuf, exp->s);

        str_clear(exp);
        str_clear(diff);
        str_clear(diff_min);

    } else {
        /* SEQ is not mapped or is not present, or CIGAR is not present or
         * this is the first sequence in the current block.
         */

        /* Output record. */
        if (pos > pow(10, 10) - 1) tsc_error("Buffer too small for POS data!\n");
        char tmp[101]; snprintf(tmp, sizeof(tmp), "%"PRIu64, pos);
        str_append_cstr(nucenc->residues, tmp);
        str_append_cstr(nucenc->residues, "\t");
        str_append_cstr(nucenc->residues, cigar);
        str_append_cstr(nucenc->residues, "\t");
        str_append_cstr(nucenc->residues, seq);
        str_append_cstr(nucenc->residues, "\n");

        /* If this record is present, add it to circular buffers. */
        if (cigar[0] != '*' && seq[0] != '*') {
            /* Expand current record. */
            nucenc_expand(exp, pos, cigar, seq);

            /* Push POS, CIGAR, SEQ and EXP to circular buffers. */
            cbufint64_push(nucenc->pos_cbuf, pos);
            cbufstr_push(nucenc->cigar_cbuf, cigar);
            cbufstr_push(nucenc->seq_cbuf, seq);
            cbufstr_push(nucenc->exp_cbuf, exp->s);
        }
    }

    str_free(exp);
    str_free(diff);
    str_free(diff_min);

    /*DEBUG("out_buf: \n%s", nucenc->residues->s);*/
    nucenc->blkl_n++;
}

static size_t nucenc_write_block(nucenc_t* nucenc, FILE* ofp)
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

