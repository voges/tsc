//
// Copyright (c) 2015
// Leibniz Universitaet Hannover, Institut fuer Informationsverarbeitung (TNT)
// Contact: Jan Voges <voges@tnt.uni-hannover.de>
//

//
// This file is part of tsc.
//
// Tsc is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Tsc is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with tsc. If not, see <http://www.gnu.org/licenses/>
//

//
// Nuc o1 block format:
//   unsigned char  blk_id[8] : "nuc----" + '\0'
//   uint64_t       rec_cnt  : No. of lines in block
//   uint64_t       data_sz  : Data size
//   uint64_t       data_crc : CRC64 of compressed data
//   unsigned char  *data    : Data
//

#include "nuccodec_o1.h"
#include "../range/range.h"
#include "../tvclib/crc64.h"
#include "../tvclib/frw.h"
#include "../tsclib.h"
#include "zlib.h"
#include <ctype.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

// Encoder
// -----------------------------------------------------------------------------

static void nucenc_init(nucenc_t* nucenc)
{
    nucenc->in_sz = 0;
    nucenc->rec_cnt = 0;
    nucenc->grec_cnt = 0;
    nucenc->first = false;
}

nucenc_t * nucenc_new(void)
{
    nucenc_t *nucenc = (nucenc_t *)tsc_malloc(sizeof(nucenc_t));

    nucenc->tmp = str_new();
    nucenc->neo_cbuf = cbufint64_new(NUCCODEC_WINDOW_SZ);
    nucenc->pos_cbuf = cbufint64_new(NUCCODEC_WINDOW_SZ);
    nucenc->exs_cbuf = cbufstr_new(NUCCODEC_WINDOW_SZ);

    nucenc_init(nucenc);

    tsc_vlog("Nucenc window size: %d\n", NUCCODEC_WINDOW_SZ);

    return nucenc;
}

void nucenc_free(nucenc_t *nucenc)
{
    if (nucenc != NULL) {
        str_free(nucenc->tmp);
        cbufint64_free(nucenc->neo_cbuf);
        cbufint64_free(nucenc->pos_cbuf);
        cbufstr_free(nucenc->exs_cbuf);

        free(nucenc);
        nucenc = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

static void nucenc_reset(nucenc_t *nucenc)
{
    str_clear(nucenc->tmp);
    cbufint64_clear(nucenc->neo_cbuf);
    cbufint64_clear(nucenc->pos_cbuf);
    cbufstr_clear(nucenc->exs_cbuf);

    nucenc->in_sz = 0;
    nucenc->rec_cnt = 0;
    nucenc->first = false;
}

static void nucenc_expand(str_t          *stogy,
                          str_t          *exs,
                          const char     *cigar,
                          const char     *seq)

{
    //DEBUG("pos: %"PRIu64" cigar: %s", pos, cigar);
    //DEBUG("seq: \t%s", seq);

    size_t cigar_idx = 0;
    size_t cigar_len = strlen(cigar);
    size_t op_len = 0; // Length of current CIGAR operation
    size_t seq_idx = 0;

    // Iterate through CIGAR string and expand SEQ
    for (cigar_idx = 0; cigar_idx < cigar_len; cigar_idx++) {
        if (isdigit(cigar[cigar_idx])) {
            op_len = op_len * 10 + cigar[cigar_idx] - '0';
            continue;
        }

        str_append_num(stogy, op_len);
        str_append_char(stogy, cigar[cigar_idx]);

        switch (cigar[cigar_idx]) {
        case 'M':
        case '=':
        case 'X':
            str_append_cstrn(exs, &seq[seq_idx], op_len);
            seq_idx += op_len;
            break;
        case 'I':
        case 'S':
            str_append_cstrn(stogy, &seq[seq_idx], op_len);
            // These are -not- added to EXS
            seq_idx += op_len;
            break;
        case 'D':
        case 'N': {
            size_t i = 0;
            for (i = 0; i < op_len; i++) { str_append_char(exs, '?'); }
            break;
        }
        case 'H':
        case 'P': {
            //size_t i = 0;
            //for (i = 0; i < op_len; i++) { str_append_char(exs, '-'); }
            break;
        }
        default: tsc_error("Bad CIGAR string: %s\n", cigar);
        }

        op_len = 0;
    }
    DEBUG("stogy: %s", stogy->s);
    DEBUG("exs: %s", exs->s);
}

static size_t nucenc_diff(uint32_t       *poff,
                          const uint32_t pos,
                          str_t          *stogy,
                          const char     *exs,
                          const uint32_t pos_ref,
                          const char     *exs_ref)
{
    // Compute and check position offset
    *poff = pos - pos_ref;
    if (*poff < 0) tsc_error("SAM file not sorted\n");
    if (*poff > strlen(exs_ref)) {
        tsc_warning("poff too large\n");
        str_append_cstr(stogy, "x");
        return stogy->len;
    }

    //DEBUG("Comparing:");
    //printf("%s\n", exs_ref);
    //int i = 0;
    //for (i = 0; i < *poff; i++) printf(" ");
    //printf("%s\n", exs);

    str_append_cstr(stogy, ":");

    // Iterate through EXS and check for indels
    size_t idx = 0;
    size_t idx_ref = *poff;
    while (exs[idx] && exs_ref[idx_ref]) {
        if (exs[idx] != exs_ref[idx_ref]) {
            str_append_num(stogy, idx);
            str_append_char(stogy, exs[idx]);
        }
        idx++;
        idx_ref++;
    }

    // Append trailing sequence
    while (exs[idx]) {
        str_append_char(stogy, exs[idx++]);
    }

    //printf("%s\n", stogy->s);

    return stogy->len;
}

void nucenc_add_record(nucenc_t       *nucenc,
                       const uint32_t pos,
                       const char     *cigar,
                       const char     *seq)
{
    nucenc->in_sz += sizeof(pos) + strlen(cigar) + strlen(seq);

    // We assume five possible symbols for seq:
    //   A=65, C=67, G=71, N=78, T=84 or a=97, c=99, g=103, n=110, t=116

    DEBUG("%d", pos);
    DEBUG("%s", cigar);
    DEBUG("%s", seq);

    // Sanity check
    if (!pos) {
        tsc_warning("POSition missing in line %zu\n", nucenc->grec_cnt);
        return;
    }
    if (!strlen(cigar) || (cigar[0] == '*' && cigar[1] == '\0')) {
        tsc_warning("CIGAR missing in line %zu\n", nucenc->grec_cnt);
        return;
    }
    if (!strlen(seq) || (seq[0] == '*' && seq[1] == '\0')) {
        tsc_warning("SEQ missing in line %zu\n", nucenc->grec_cnt);
        return;
    }

    // Convert SEQ to uppercase
    //size_t i = 0;
    //while (seq[i++]) { seq[i] = toupper(seq[i]); }

    // First record in block
    if (!nucenc->first) {
        str_t* stogy = str_new();
        str_t* exs = str_new();
        nucenc_expand(stogy, exs, cigar, seq);

        cbufint64_push(nucenc->neo_cbuf, strlen(cigar));
        cbufint64_push(nucenc->pos_cbuf, pos);
        cbufstr_push(nucenc->exs_cbuf, exs->s);

        str_append_num(nucenc->tmp, pos);
        str_append_cstr(nucenc->tmp, ":");
        str_append_str(nucenc->tmp, stogy);
        str_append_cstr(nucenc->tmp, ":");
        str_append_cstr(nucenc->tmp, seq);
        str_append_cstr(nucenc->tmp, "~");

        str_free(stogy);
        str_free(exs);

        nucenc->first = true;
        nucenc->rec_cnt++;
        return;
    }

    // 1) Expand SEQ using and CIGAR. This yields an expanded sequence (EXS)
    //    and a modified CIGAR string (STOGY).
    str_t* stogy = str_new();
    str_t* exs = str_new();
    nucenc_expand(stogy, exs, cigar, seq);

    // 2) Get matching NEO, POS, STOGY, EXS from circular buffer
    size_t cbuf_idx = 0;
    size_t cbuf_n = nucenc->pos_cbuf->n;
    size_t cbuf_idx_best = cbuf_idx;
    uint32_t diff_best = UINT32_MAX;

    do {
        int64_t neo_ref = cbufint64_get(nucenc->neo_cbuf, cbuf_idx);
        int64_t diff = neo_ref; // TODO
        if (diff < diff_best) {
            diff_best = diff;
            cbuf_idx_best = cbuf_idx;
        }
        cbuf_idx++;
    } while (cbuf_idx < cbuf_n);

    int64_t pos_ref = cbufint64_get(nucenc->pos_cbuf, cbuf_idx_best);
    str_t   *exs_ref = cbufstr_get(nucenc->exs_cbuf, cbuf_idx_best);

    // 3) Compute changes (indels) from ref to curr and extend STOGY
    uint32_t poff;
    size_t neo = nucenc_diff(&poff, pos, stogy, exs->s, pos_ref, exs_ref->s);

    // 4) Push NEO, POS, STOGY, EXS into circular buffer
    cbufint64_push(nucenc->neo_cbuf, neo);
    cbufint64_push(nucenc->pos_cbuf, pos);
    cbufstr_push(nucenc->exs_cbuf, exs->s);

    // 5) Write STOGY
    str_append_num(nucenc->tmp, poff);
    str_append_cstr(nucenc->tmp, ":");
    str_append_str(nucenc->tmp, stogy);
    str_append_cstr(nucenc->tmp, "~");
    DEBUG("%s", nucenc->tmp->s);

    str_free(stogy);
    str_free(exs);

    nucenc->rec_cnt++;
    nucenc->grec_cnt++;
}

size_t nucenc_write_block(nucenc_t *nucenc, FILE *fp)
{
    size_t ret = 0;

    // Compress block with range coder
    //unsigned char *tmp = (unsigned char *)nucenc->tmp->s;
    //unsigned int tmp_sz = (unsigned int)nucenc->tmp->len;
    //unsigned int data_sz = 0;
    //unsigned char *data = range_compress_o0(tmp, tmp_sz, &data_sz);

    // Compress block with zlib
    unsigned char *tmp = (unsigned char *)nucenc->tmp->s;
    unsigned int tmp_sz = (unsigned int)nucenc->tmp->len;
    Byte *data;
    uLong data_sz = 100000*sizeof(int);
    data = (Byte *)calloc((uInt)data_sz, 1);
    int err = compress(data, &data_sz, (const Bytef *)tmp, (uLong)tmp_sz);
    if (err != Z_OK) tsc_error("zlib failed to compress: %d\n", err);
    // TODO: Clean this up

    // Compute CRC64
    uint64_t data_crc = crc64(data, data_sz);

    // Write compressed block
    unsigned char blk_id[8] = "nuc----"; blk_id[7] = '\0';
    ret += fwrite_buf(fp, blk_id, sizeof(blk_id));
    ret += fwrite_uint64(fp, (uint64_t)nucenc->rec_cnt);
    ret += fwrite_uint64(fp, (uint64_t)data_sz);
    ret += fwrite_uint64(fp, (uint64_t)data_crc);
    ret += fwrite_buf(fp, data, data_sz);

    tsc_vlog("Compressed nuc block: %zu bytes -> %zu bytes (%6.2f%%)\n",
             nucenc->in_sz,
             data_sz,
             (double)data_sz / (double)nucenc->in_sz * 100);

    nucenc_reset(nucenc); // Reset encoder for next block
    free(data); // Free memory used for encoded bitstream

    return ret;
}

// Decoder
// -----------------------------------------------------------------------------

static void nucdec_init(nucdec_t *nucdec)
{
    nucdec->out_sz = 0;
}

nucdec_t * nucdec_new(void)
{
    nucdec_t *nucdec = (nucdec_t *)tsc_malloc(sizeof(nucdec_t));
    nucdec_init(nucdec);
    return nucdec;
}

void nucdec_free(nucdec_t *nucdec)
{
    if (nucdec != NULL) {
        free(nucdec);
        nucdec = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

static void nucdec_reset(nucdec_t *nucdec)
{
    nucdec_init(nucdec);
}

static size_t nucdec_decode(nucdec_t      *nucdec,
                            unsigned char *tmp,
                            size_t        tmp_sz,
                            uint32_t      *pos,
                            str_t         **cigar,
                            str_t         **seq)
{
    return 0;
}

size_t nucdec_decode_block(nucdec_t *nucdec,
                           FILE     *fp,
                           uint32_t *pos,
                           str_t    **cigar,
                           str_t    **seq)
{
    unsigned char  blk_id[8];
    uint64_t       rec_cnt;
    uint64_t       data_sz;
    uint64_t       data_crc;
    unsigned char  *data;

    // Read block
    fread_buf(fp, blk_id, sizeof(blk_id));
    fread_uint64(fp, &rec_cnt);
    fread_uint64(fp, &data_sz);
    fread_uint64(fp, &data_crc);
    data = (unsigned char *)tsc_malloc((size_t)data_sz);
    fread_buf(fp, data, data_sz);

    // Compute tsc block size
    size_t ret = sizeof(blk_id)
               + sizeof(rec_cnt)
               + sizeof(data_sz)
               + sizeof(data_crc)
               + data_sz;

    // Check CRC64
    if (crc64(data, data_sz) != data_crc)
        tsc_error("CRC64 check failed for nuc block\n");

    // Decompress block
    unsigned int tmp_sz = 0;
    unsigned char *tmp = range_decompress_o0(data, data_sz, &tmp_sz);
    free(data);

    // Decode block
    nucdec->out_sz = nucdec_decode(nucdec, tmp, tmp_sz, pos, cigar, seq);
    free(tmp); // Free memory used for decoded bitstream

    tsc_vlog("Decompressed nuc block: %zu bytes -> %zu bytes (%6.2f%%)\n",
             data_sz,
             nucdec->out_sz,
             (double)nucdec->out_sz / (double)data_sz * 100);

    nucdec_reset(nucdec);

    return ret;
}

