/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2015, Leibniz Universitaet Hannover, Institut fuer
 * Informationsverarbeitung (TNT)
 * Contact: <voges@tnt.uni-hannover.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the TNT nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// Nuc o1 block format:
//   unsigned char  blk_id[8]  : "nuc----" + '\0'
//   uint64_t       rec_blk_cnt: No. of lines in block
//   uint64_t       tmp_sz     : Size of uncompressed data
//   uint64_t       data_sz    : Data size
//   uint64_t       data_crc   : CRC64 of compressed data
//   unsigned char  *data      : Data
//

#include "nuccodec_o1.h"
#include "../range.h"
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

static void nucenc_init(nucenc_t *nucenc)
{
    nucenc->in_sz = 0;
    nucenc->rec_blk_cnt = 0;
    nucenc->rec_tot_cnt = 0;
    nucenc->first = false;

    nucenc->m_blk_cnt = 0;
    nucenc->m_tot_cnt = 0;
    nucenc->i_blk_cnt = 0;
    nucenc->i_tot_cnt = 0;
    nucenc->stogy_mu = 0.0;
    nucenc->mod_mu = 0.0;
    nucenc->trail_mu = 0.0;

    // Open statistics file and write header for it
    str_append_str(nucenc->stat_fname, tsc_out_fname);
    str_append_cstr(nucenc->stat_fname, ".nuc.log");
    nucenc->stat_fp = tsc_fopen((const char *)nucenc->stat_fname->s, "w");
    str_t *stat = str_new();
    str_append_cstr(stat, "m_blk_cnt,");
    str_append_cstr(stat, "i_blk_cnt,");
    str_append_cstr(stat, "stogy_mu,");
    str_append_cstr(stat, "mod_mu,");
    str_append_cstr(stat, "trail_mu,");
    str_append_cstr(stat,"CR\n");
    fwrite_buf(nucenc->stat_fp, (const unsigned char *)stat->s, stat->len);
    str_free(stat);
}

nucenc_t * nucenc_new(void)
{
    nucenc_t *nucenc = (nucenc_t *)tsc_malloc(sizeof(nucenc_t));

    nucenc->tmp = str_new();
    nucenc->rname_prev = str_new();
    nucenc->stat_fname = str_new();
    nucenc->neo_cbuf = cbufint64_new(WINDOW_SZ);
    nucenc->pos_cbuf = cbufint64_new(WINDOW_SZ);
    nucenc->exs_cbuf = cbufstr_new(WINDOW_SZ);

    nucenc_init(nucenc);

    tsc_log(TSC_LOG_INFO, "Nuccodec WINDOW_SZ: %d\n", WINDOW_SZ);
    tsc_log(TSC_LOG_INFO, "Nuccodec ALPHA: %.1f\n", ALPHA);

    return nucenc;
}

void nucenc_free(nucenc_t *nucenc)
{
    tsc_fclose(nucenc->stat_fp);
    tsc_log(TSC_LOG_INFO, "Nuccodec statistics: %s\n", nucenc->stat_fname->s);

    if (nucenc != NULL) {
        str_free(nucenc->tmp);
        str_free(nucenc->rname_prev);
        str_free(nucenc->stat_fname);
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
    str_clear(nucenc->rname_prev);
    cbufint64_clear(nucenc->neo_cbuf);
    cbufint64_clear(nucenc->pos_cbuf);
    cbufstr_clear(nucenc->exs_cbuf);
    nucenc->in_sz = 0;
    nucenc->rec_blk_cnt = 0;
    nucenc->first = false;
    nucenc->m_blk_cnt = 0;
    nucenc->i_blk_cnt = 0;
    nucenc->stogy_mu = 0.0;
    nucenc->mod_mu = 0.0;
    nucenc->trail_mu = 0.0;
}

static void nucenc_expand(str_t      *stogy,
                          str_t      *exs,
                          const char *cigar,
                          const char *seq)

{
    size_t cigar_idx = 0;
    size_t cigar_len = strlen(cigar);
    size_t op_len = 0; // Length of current CIGAR operation
    size_t seq_idx = 0;

    //
    // Iterate through CIGAR string and expand SEQ; this yields EXS and STOGY
    //

    for (cigar_idx = 0; cigar_idx < cigar_len; cigar_idx++) {
        if (isdigit(cigar[cigar_idx])) {
            op_len = op_len * 10 + cigar[cigar_idx] - '0';
            continue;
        }

        // Copy CIGAR part to STOGY
        str_append_int(stogy, op_len);
        str_append_char(stogy, cigar[cigar_idx]);

        switch (cigar[cigar_idx]) {
        case 'M':
        case '=':
        case 'X':
            // Add matching part to EXS
            str_append_cstrn(exs, &seq[seq_idx], op_len);
            seq_idx += op_len;
            break;
        case 'I':
        case 'S':
            // Add inserted part to STOGY (-not- to EXS)
            str_append_cstrn(stogy, &seq[seq_idx], op_len);
            seq_idx += op_len;
            break;
        case 'D':
        case 'N': {
            // Inflate EXS
            size_t i = 0;
            for (i = 0; i < op_len; i++) { str_append_char(exs, '?'); }
            break;
        }
        case 'H':
        case 'P': {
            // These have been clipped
            break;
        }
        default: tsc_error("Bad CIGAR string: %s\n", cigar);
        }

        op_len = 0;
    }
}

static void nucenc_diff(str_t          *mod,
                        str_t          *trail,
                        const uint32_t pos_off,
                        const char     *exs,
                        const char     *exs_ref)
{
    //
    // Iterate through EXS, check for modifications with respect to EXS_REF
    // and store them in MOD
    //

    size_t idx = 0;
    size_t idx_ref = pos_off;
    while (exs[idx] && exs_ref[idx_ref]) {
        if (exs[idx] != exs_ref[idx_ref]) {
            str_append_int(mod, idx);
            str_append_char(mod, exs[idx]);
        }
        idx++;
        idx_ref++;
    }

    //
    // Append trailing sequence to TRAIL
    //

    while (exs[idx]) {
        str_append_char(trail, exs[idx++]);
    }
}

void nucenc_add_record(nucenc_t       *nucenc,
                       const char     *rname,
                       const uint32_t pos,
                       const char     *cigar,
                       const char     *seq)
{
    nucenc->in_sz += strlen(rname) + sizeof(pos) + strlen(cigar) + strlen(seq);
    nucenc->rec_blk_cnt++;
    nucenc->rec_tot_cnt++;

    //
    // Sanity check:
    // - Write mRNAME:POS:CIGAR:SEQ~ in case of corrupted record
    // - Do -not- push to circular buffer
    //

    if (   (!strlen(rname) || (rname[0] == '*' && rname[1] == '\0'))
        || (!pos)
        || (!strlen(cigar) || (cigar[0] == '*' && cigar[1] == '\0'))
        || (!strlen(seq) || (seq[0] == '*' && seq[1] == '\0'))) {
        tsc_log(TSC_LOG_WARN,
                "Missing RNAME|POS|CIGAR|SEQ (line %zu)\n",
                nucenc->rec_tot_cnt);
        nucenc->m_blk_cnt++;

        str_append_cstr(nucenc->tmp, "m");
        str_append_cstr(nucenc->tmp, rname);
        str_append_cstr(nucenc->tmp, ":");
        str_append_int(nucenc->tmp, pos);
        str_append_cstr(nucenc->tmp, ":");
        str_append_cstr(nucenc->tmp, cigar);
        str_append_cstr(nucenc->tmp, ":");
        str_append_cstr(nucenc->tmp, seq);
        str_append_cstr(nucenc->tmp, "~");

        return;
    }

    //
    // First record in block:
    // - Store RNAME
    // - Expand SEQ using CIGAR
    // - Write fRNAME:POS:STOGY:EXS~
    // - Push NEO, POS, EXS to circular buffer
    //

    if (!nucenc->first) {
        str_t *stogy = str_new();
        str_t *exs = str_new();

        str_copy_cstr(nucenc->rname_prev, rname);

        nucenc_expand(stogy, exs, cigar, seq);

        str_append_cstr(nucenc->tmp, "f");
        str_append_cstr(nucenc->tmp, rname);
        str_append_cstr(nucenc->tmp, ":");
        str_append_int(nucenc->tmp, pos);
        str_append_cstr(nucenc->tmp, ":");
        str_append_str(nucenc->tmp, stogy);
        str_append_cstr(nucenc->tmp, ":");
        str_append_str(nucenc->tmp, exs);
        str_append_cstr(nucenc->tmp, "~");

        cbufint64_push(nucenc->neo_cbuf, ceil(ALPHA*stogy->len));
        cbufint64_push(nucenc->pos_cbuf, pos);
        cbufstr_push(nucenc->exs_cbuf, exs->s);

        str_free(stogy);
        str_free(exs);

        nucenc->first = true;
        return;
    }

    //
    // Record passed sanity check, and is not the 1st record
    // - Expand SEQ using CIGAR
    // - Get matching EXS from circular buffer
    // - Check position offset
    // - Either make new I-Record or encode "normal" record
    //

    str_t *stogy = str_new();
    str_t *mod = str_new();
    str_t *trail = str_new();
    str_t *exs = str_new();

    nucenc_expand(stogy, exs, cigar, seq);

    // Get matching NEO, POS, EXS from circular buffer
    size_t cbuf_idx = 0;
    size_t cbuf_n = nucenc->neo_cbuf->n;
    size_t cbuf_idx_best = cbuf_idx;
    uint32_t neo_best = UINT32_MAX;

    do {
        uint32_t neo_ref = (uint32_t)cbufint64_get(nucenc->neo_cbuf, cbuf_idx);
        if (neo_ref < neo_best) {
            neo_best = neo_ref;
            cbuf_idx_best = cbuf_idx;
        }
        cbuf_idx++;
    } while (cbuf_idx < cbuf_n);

    int64_t pos_ref = cbufint64_get(nucenc->pos_cbuf, cbuf_idx_best);
    str_t *exs_ref = cbufstr_get(nucenc->exs_cbuf, cbuf_idx_best);

    // Compute and check position offset
    int64_t pos_off = pos - pos_ref;
    uint32_t neo = 0;

    if (   pos_off > exs_ref->len
        || pos_off < 0
        || strcmp(rname, nucenc->rname_prev->s)) {
        nucenc->i_blk_cnt++;

        //
        // Position offset is negative or too large, or RNAME has changed:
        // - Store RNAME
        // - Write iRNAME:POS:STOGY:EXS
        // - Clear circular buffer
        // - Push NEO, POS, EXS to circular buffer
        //

        str_copy_cstr(nucenc->rname_prev, rname);

        str_append_cstr(nucenc->tmp, "i");
        str_append_cstr(nucenc->tmp, rname);
        str_append_cstr(nucenc->tmp, ":");
        str_append_int(nucenc->tmp, pos);
        str_append_cstr(nucenc->tmp, ":");
        str_append_str(nucenc->tmp, stogy);
        str_append_cstr(nucenc->tmp, ":");
        str_append_str(nucenc->tmp, exs);
        str_append_cstr(nucenc->tmp, "~");

        cbufint64_clear(nucenc->neo_cbuf);
        cbufint64_clear(nucenc->pos_cbuf);
        cbufstr_clear(nucenc->exs_cbuf);

        cbufint64_push(nucenc->neo_cbuf, ceil(ALPHA*stogy->len));
        cbufint64_push(nucenc->pos_cbuf, pos);
        cbufstr_push(nucenc->exs_cbuf, exs->s);
    } else {

        //
        // "Normal" record
        // - Compute MOD and TRAIL
        // - Write POS_OFF:STOGY[:MOD][:TRAIL]~
        // - Push NEO, POS, and EXS to circular buffer
        //

        nucenc_diff(mod, trail, pos_off, exs->s, exs_ref->s);

        str_append_int(nucenc->tmp, pos_off);
        str_append_cstr(nucenc->tmp, ":");
        str_append_str(nucenc->tmp, stogy);
        if (mod->len) {
            str_append_cstr(nucenc->tmp, ":");
            str_append_str(nucenc->tmp, mod);
        }
        if (trail->len) {
            str_append_cstr(nucenc->tmp, ":");
            str_append_str(nucenc->tmp, trail);
        }
        str_append_cstr(nucenc->tmp, "~");

        neo = ceil((1-ALPHA)*pos_off + ALPHA*(stogy->len + mod->len));
        cbufint64_push(nucenc->neo_cbuf, neo);
        cbufint64_push(nucenc->pos_cbuf, pos);
        cbufstr_push(nucenc->exs_cbuf, exs->s);
    }

    // Update statistics
    nucenc->stogy_mu += stogy->len;
    nucenc->mod_mu += mod->len;
    nucenc->trail_mu += trail->len;

    str_free(stogy);
    str_free(mod);
    str_free(trail);
    str_free(exs);
}

size_t nucenc_write_block(nucenc_t *nucenc, FILE *fp)
{
    size_t ret = 0;

    // Compress block
    unsigned char *tmp = (unsigned char *)nucenc->tmp->s;
    unsigned int tmp_sz = (unsigned int)nucenc->tmp->len;
    Byte *data;
    compressBound(tmp_sz);
    uLong data_sz = compressBound(tmp_sz) + 1;
    data = (Byte *)calloc((uInt)data_sz, 1);
    int err = compress(data, &data_sz, (const Bytef *)tmp, (uLong)tmp_sz);
    if (err != Z_OK) tsc_error("zlib failed to compress: %d\n", err);

    // Compute CRC64
    uint64_t data_crc = crc64(data, data_sz);

    // Write compressed block
    unsigned char blk_id[8] = "nuc----"; blk_id[7] = '\0';
    ret += fwrite_buf(fp, blk_id, sizeof(blk_id));
    ret += fwrite_uint64(fp, (uint64_t)nucenc->rec_blk_cnt);
    ret += fwrite_uint64(fp, (uint64_t)tmp_sz);
    ret += fwrite_uint64(fp, (uint64_t)data_sz);
    ret += fwrite_uint64(fp, (uint64_t)data_crc);
    ret += fwrite_buf(fp, data, data_sz);

    tsc_log(TSC_LOG_VERBOSE,
            "Compressed nuc block: %zu bytes -> %zu bytes (%6.2f%%)\n",
            nucenc->in_sz,
            data_sz,
            (double)data_sz / (double)nucenc->in_sz * 100);

    nucenc->m_tot_cnt += nucenc->m_blk_cnt;
    nucenc->i_tot_cnt += nucenc->i_blk_cnt;

    // Write statistics to file
    str_t *stat = str_new();
    str_append_int(stat, nucenc->m_blk_cnt);
    str_append_cstr(stat, ",");
    str_append_int(stat, nucenc->i_blk_cnt);
    str_append_cstr(stat, ",");
    str_append_double2(stat, nucenc->stogy_mu/(double)nucenc->rec_blk_cnt);
    str_append_cstr(stat, ",");
    str_append_double2(stat, nucenc->mod_mu/(double)nucenc->rec_blk_cnt);
    str_append_cstr(stat, ",");
    str_append_double2(stat, nucenc->trail_mu/(double)nucenc->rec_blk_cnt);
    str_append_cstr(stat, ",");
    str_append_double2(stat,(double)data_sz / (double)nucenc->in_sz * 100);
    str_append_cstr(stat, "\n");
    fwrite_buf(nucenc->stat_fp, (const unsigned char *)stat->s, stat->len);
    str_free(stat);

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
    nucdec->neo_cbuf = cbufint64_new(WINDOW_SZ);
    nucdec->pos_cbuf = cbufint64_new(WINDOW_SZ);
    nucdec->exs_cbuf = cbufstr_new(WINDOW_SZ);
    nucdec_init(nucdec);
    return nucdec;
}

void nucdec_free(nucdec_t *nucdec)
{
    if (nucdec != NULL) {
        cbufint64_free(nucdec->neo_cbuf);
        cbufint64_free(nucdec->pos_cbuf);
        cbufstr_free(nucdec->exs_cbuf);
        free(nucdec);
        nucdec = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

static void nucdec_reset(nucdec_t *nucdec)
{
    cbufint64_clear(nucdec->neo_cbuf);
    cbufint64_clear(nucdec->pos_cbuf);
    cbufstr_clear(nucdec->exs_cbuf);
    nucdec_init(nucdec);
}

static void nucdec_contract(str_t      *cigar,
                            str_t      *seq,
                            const char *stogy,
                            const char *exs)
{
    size_t stogy_idx = 0;
    size_t stogy_len = strlen(stogy);
    size_t op_len = 0; // Length of current STOGY operation
    size_t exs_idx = 0;

    //
    // Iterate through STOGY string regenerate CIGAR and SEQ from STOGY and EXS
    //

    for (stogy_idx = 0; stogy_idx < stogy_len; stogy_idx++) {
        if (isdigit(stogy[stogy_idx])) {
            op_len = op_len * 10 + stogy[stogy_idx] - '0';
            continue;
        }

        // Regenerate CIGAR
        str_append_int(cigar, op_len);
        str_append_char(cigar, stogy[stogy_idx]);

        switch (stogy[stogy_idx]) {
        case 'M':
        case '=':
        case 'X':
            // Copy matching part from EXS to SEQ
            str_append_cstrn(seq, &exs[exs_idx], op_len);
            exs_idx += op_len;
            break;
        case 'I':
        case 'S':
            // Copy inserted part from STOGY to SEQ and move STOGY pointer
            str_append_cstrn(seq, &stogy[stogy_idx+1], op_len);
            stogy_idx += op_len;
            break;
        case 'D':
        case 'N': {
            // EXS was inflated; move pointer
            exs_idx += op_len;
            break;
        }
        case 'H':
        case 'P': {
            // These have been clipped
            break;
        }
        default: tsc_error("Bad STOGY string: %s\n", stogy);
        }

        op_len = 0;
    }
}

static void nucdec_alike(str_t          *exs,
                         const char     *exs_ref,
                         const uint32_t pos_off,
                         const char     *stogy,
                         const char     *mod,
                         const char     *trail)
{
    // Copy matching part from EXS_REF to EXS
    size_t stogy_idx = 0;
    size_t stogy_len = strlen(stogy);
    size_t op_len = 0;
    size_t match_len = 0;
    for (stogy_idx = 0; stogy_idx < stogy_len; stogy_idx++) {
        if (isdigit(stogy[stogy_idx])) {
            op_len = op_len * 10 + stogy[stogy_idx] - '0';
            continue;
        }

        match_len += op_len;

        // Reduce match length if bases have been inserted or soft-clipped
        switch (stogy[stogy_idx]) {
        case 'I':
        case 'S':
            match_len -= op_len;
            break;
        default:
            break;
        }

        op_len = 0;
    }

    if (match_len+pos_off >= strlen(exs_ref)) {
        match_len = strlen(exs_ref)-pos_off;
    }

    str_append_cstrn(exs, &exs_ref[pos_off], match_len);

    // Integrate MODs into EXS
    size_t mod_idx = 0;
    size_t mod_len = strlen(mod);
    size_t mod_pos = 0;

    for (mod_idx = 0; mod_idx < mod_len; mod_idx++) {
        if (isdigit(mod[mod_idx])) {
            mod_pos = mod_pos * 10 + mod[mod_idx] - '0';
            continue;
        }

        exs->s[mod_pos] = mod[mod_idx];
        mod_pos = 0;
    }

    // Append TRAIL to EXS
    str_append_cstr(exs, trail);
}

static size_t nucdec_decode_records(nucdec_t      *nucdec,
                                    unsigned char *tmp,
                                    size_t        tmp_sz,
                                    str_t         **rname,
                                    uint32_t      *pos,
                                    str_t         **cigar,
                                    str_t         **seq)
{
    size_t ret = 0;

    str_t *rec = str_new();
    size_t rec_blk_cnt = 0;
    str_t *rname_prev = str_new();

    while (*tmp != '\0') {
        // Get tsc record
        while (*tmp != '~') {
            str_append_char(rec, *tmp++);
        }
        str_append_char(rec, *tmp);

        // Make readable pointer aliases for current record
        str_t *_rname_ = rname[rec_blk_cnt];
        uint32_t *_pos_ = &pos[rec_blk_cnt];
        str_t *_cigar_ = cigar[rec_blk_cnt];
        str_t *_seq_ = seq[rec_blk_cnt];

        //
        // Check for skipped records of type mRNAME:POS:CIGAR:SEQ]~
        //

        if (rec->s[0] == 'm') {
            int itr = 1;

            // Get RNAME
            while (rec->s[itr] != ':' && rec->s[itr] != '~') {
                str_append_char(_rname_, rec->s[itr]);
                ++itr;
            }
            if (!_rname_->len) str_append_char(_rname_, '*');

            // Get POS
            *_pos_ = 0;
            itr++;
            while (rec->s[itr] != ':') {
                *_pos_ = *_pos_ * 10 + rec->s[itr] - '0';
                ++itr;
            }

            // Get CIGAR
            itr++;
            while (rec->s[itr] != ':') {
                str_append_char(_cigar_, rec->s[itr]);
                ++itr;
            }
            if (!_cigar_->len) str_append_char(_cigar_, '*');

            // Get SEQ
            itr++;
            while (rec->s[itr] != '~') {
                str_append_char(_seq_, rec->s[itr]);
                ++itr;
            }
            if (!_seq_->len) str_append_char(_seq_, '*');
        }

        //
        // Get first record of type fRNAME:POS:STOGY:EXS~ or inserted I-Record
        // of type iRNAME:POS:STOGY:EXS~
        //

        if (rec->s[0] == 'f' || rec->s[0] == 'i') {
            str_t *stogy = str_new();
            str_t *exs = str_new();
            int itr = 1;

            // Get RNAME
            while (rec->s[itr] != ':') {
                str_append_char(_rname_, rec->s[itr]);
                itr++;
            }

            // Get POS
            *_pos_ = 0;
            ++itr;
            while (rec->s[itr] != ':') {
                *_pos_ = *_pos_ * 10 + rec->s[itr] - '0';
                itr++;
            }

            // Get STOGY
            itr++;
            while (rec->s[itr] != ':') {
                str_append_char(stogy, rec->s[itr]);
                itr++;
            }

            // Get EXS
            itr++;
            while (rec->s[itr] != '~') {
                str_append_char(exs, rec->s[itr]);
                itr++;
            }

            // Clear circular buffers
            cbufint64_clear(nucdec->neo_cbuf);
            cbufint64_clear(nucdec->pos_cbuf);
            cbufstr_clear(nucdec->exs_cbuf);

            // Push NEO, POS, EXS to circular buffer
            cbufint64_push(nucdec->neo_cbuf, (int64_t)ceil(ALPHA*stogy->len));
            cbufint64_push(nucdec->pos_cbuf, *_pos_);
            cbufstr_push(nucdec->exs_cbuf, exs->s);

            // Contract
            nucdec_contract(_cigar_, _seq_, stogy->s, exs->s);

            // Store RNAME
            str_copy_str(rname_prev, _rname_);

            str_free(stogy);
            str_free(exs);
        }

        //
        // This is a "normal" record of type POS_OFF:STOGY[:MOD][:TRAIL]~
        //

        if (isdigit(rec->s[0])) {
            int64_t pos_off = 0;
            str_t *stogy = str_new();
            str_t *exs = str_new();
            str_t *mod = str_new();
            str_t *trail = str_new();

            // Get RNAME
            str_copy_str(_rname_, rname_prev);

            int itr = 0;

            // Get POS_OFF
            while (rec->s[itr] != ':') {
                pos_off = pos_off * 10 + rec->s[itr] - '0';
                ++itr;
            }

            // Get STOGY
            itr++;
            while (rec->s[itr] != ':' && rec->s[itr] != '~') {
                str_append_char(stogy, rec->s[itr]);
                ++itr;
            }

            // Get MOD
            if (rec->s[itr] != '~') itr++;
            if (isdigit(rec->s[itr])) {
                while (rec->s[itr] != ':' && rec->s[itr] != '~') {
                    str_append_char(mod, rec->s[itr]);
                    ++itr;
                }
            }
            if (!mod->len) itr--;

            // Get TRAIL
            if (rec->s[itr] != '~') itr++;
            while (rec->s[itr] != '~') {
                str_append_char(trail, rec->s[itr]);
                ++itr;
            }

            // Compute NEO
            uint32_t neo = (uint32_t)ceil((1-ALPHA)*pos_off + ALPHA*(stogy->len + mod->len));

            // Get matching NEO, POS, EXS from circular buffer
            size_t cbuf_idx = 0;
            size_t cbuf_n = nucdec->pos_cbuf->n;
            size_t cbuf_idx_best = cbuf_idx;
            uint32_t neo_best = UINT32_MAX;

            do {
                uint32_t neo_ref = (uint32_t)cbufint64_get(nucdec->neo_cbuf, cbuf_idx);
                if (neo_ref < neo_best) {
                    neo_best = neo_ref;
                    cbuf_idx_best = cbuf_idx;
                }
                cbuf_idx++;
            } while (cbuf_idx < cbuf_n);

            int64_t pos_ref = cbufint64_get(nucdec->pos_cbuf, cbuf_idx_best);
            str_t *exs_ref = cbufstr_get(nucdec->exs_cbuf, cbuf_idx_best);

            // Compute POS
            *_pos_ = pos_off + pos_ref;

            // Reintegrate MOD and TRAIL into EXS
            nucdec_alike(exs, exs_ref->s, pos_off, stogy->s, mod->s, trail->s);

            // Push NEO, POS, EXS to circular buffer
            cbufint64_push(nucdec->neo_cbuf, neo);
            cbufint64_push(nucdec->pos_cbuf, *_pos_);
            cbufstr_push(nucdec->exs_cbuf, exs->s);

            // Contract EXS
            nucdec_contract(_cigar_, _seq_, stogy->s, exs->s);

            str_free(stogy);
            str_free(exs);
            str_free(mod);
            str_free(trail);
        }

        ret += _rname_->len + sizeof(*_pos_) + _cigar_->len + _seq_->len;
        str_clear(rec);
        rec_blk_cnt++;
        tmp++;
    }

    str_free(rec);
    str_free(rname_prev);

    return ret;
}

size_t nucdec_decode_block(nucdec_t *nucdec,
                           FILE     *fp,
                           str_t    **rname,
                           uint32_t *pos,
                           str_t    **cigar,
                           str_t    **seq)
{
    unsigned char  blk_id[8];
    uint64_t       rec_blk_cnt;
    uint64_t       tmp_sz;
    uint64_t       data_sz;
    uint64_t       data_crc;
    unsigned char  *data;

    // Read block
    fread_buf(fp, blk_id, sizeof(blk_id));
    fread_uint64(fp, &rec_blk_cnt);
    fread_uint64(fp, &tmp_sz);
    fread_uint64(fp, &data_sz);
    fread_uint64(fp, &data_crc);
    data = (unsigned char *)tsc_malloc((size_t)data_sz);
    fread_buf(fp, data, data_sz);

    // Compute tsc block size
    size_t ret = sizeof(blk_id)
               + sizeof(rec_blk_cnt)
               + sizeof(tmp_sz)
               + sizeof(data_sz)
               + sizeof(data_crc)
               + data_sz;

    // Check CRC64
    if (crc64(data, data_sz) != data_crc)
        tsc_error("CRC64 check failed for nuc block\n");

    // Decompress block
    Bytef *tmp = (Bytef *)tsc_malloc(tmp_sz * sizeof(unsigned char));
    int err = uncompress(tmp, (uLongf *)&tmp_sz, (const Bytef *)data, (uLong)data_sz);
    if (err != Z_OK) tsc_error("zlib failed to uncompress: %d\n", err);
    free(data);

    // Decode block
    tmp = tsc_realloc(tmp, ++tmp_sz);
    tmp[tmp_sz-1] = '\0'; // Terminate tmp
    nucdec->out_sz = nucdec_decode_records(nucdec, tmp, tmp_sz, rname, pos, cigar, seq);
    free(tmp); // Free memory used for decoded bitstream

    tsc_log(TSC_LOG_VERBOSE,
            "Decompressed nuc block: %zu bytes -> %zu bytes (%6.2f%%)\n",
            data_sz,
            nucdec->out_sz,
            (double)nucdec->out_sz / (double)data_sz * 100);

    nucdec_reset(nucdec);

    return ret;
}

