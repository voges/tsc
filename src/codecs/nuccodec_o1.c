/*
 * The copyright in this software is being made available under the TNT
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2015, Leibniz Universitaet Hannover, Institut fuer
 * Informationsverarbeitung (TNT)
 * Contact: <voges@tnt.uni-hannover.de>
 * All rights reserved.
 *
 * * Redistribution in source or binary form is not permitted.
 *
 * * Use in source or binary form is only permitted in the context of scientific
 *   research.
 *
 * * Commercial use without specific prior written permission is prohibited.
 *   Neither the name of the TNT nor the names of its contributors may be used
 *   to endorse or promote products derived from this software without specific
 *   prior written permission.
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
//   unsigned char id[8]     : "nuc----" + '\0'
//   uint64_t      record_cnt   : No. of lines in block
//   CTRL
//     uint64_t      ctrl_sz       : Size of uncompressed data
//     uint64_t      ctrl_compressed_sz  : Compressed data size
//     uint64_t      ctrl_compressed_crc : CRC64 of compressed data
//     unsigned char *ctrl_compressed    : Compressed data
//   POFF
//     uint64_t      poff_sz       : Size of uncompressed data
//     uint64_t      poff_compressed_sz  : Compressed data size
//     uint64_t      poff_compressed_crc : CRC64 of compressed data
//     unsigned char *poff_compressed    : Compressed data
//   STOGY
//     uint64_t      stogy_sz      : Size of uncompressed data
//     uint64_t      stogy_compressed_sz : Compressed data size
//     uint64_t      stogy_compressed_crc: CRC64 of compressed data
//     unsigned char *stogy_compressed   : Compressed data
//   MOD
//     uint64_t      mod_sz        : Size of uncompressed data
//     uint64_t      mod_compressed_sz   : Compressed data size
//     uint64_t      mod_compressed_crc  : CRC64 of compressed data
//     unsigned char *mod_compressed     : Compressed data
//   TRAIL
//     uint64_t      trail_sz      : Size of uncompressed data
//     uint64_t      trail_compressed_sz : DCompressed data size
//     uint64_t      trail_compressed_crc: CRC64 of compressed data
//     unsigned char *trail_compressed   : Compressed data
//

#include "nuccodec_o1.h"
#include "common/crc64.h"
#include "tnt.h"
#include "tsclib.h"
#include "zlib-wrap.h"
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
    nucenc->record_cnt = 0;
    nucenc->first = false;
}

nucenc_t * nucenc_new(void)
{
    nucenc_t *nucenc = (nucenc_t *)tnt_malloc(sizeof(nucenc_t));

    nucenc->rname_prev = str_new();

    nucenc->ctrl = str_new();
    nucenc->poff = str_new();
    nucenc->stogy = str_new();
    nucenc->mod = str_new();
    nucenc->trail = str_new();

    nucenc->neo_cbuf = cbufint64_new(WINDOW_SZ);
    nucenc->pos_cbuf = cbufint64_new(WINDOW_SZ);
    nucenc->exs_cbuf = cbufstr_new(WINDOW_SZ);

    nucenc_init(nucenc);

    DEBUG("Nuccodec WINDOW_SZ: %d\n", WINDOW_SZ);
    DEBUG("Nuccodec ALPHA: %.1f\n", ALPHA);

    return nucenc;
}

void nucenc_free(nucenc_t *nucenc)
{
    if (nucenc != NULL) {
        str_free(nucenc->rname_prev);
        str_free(nucenc->ctrl);
        str_free(nucenc->poff);
        str_free(nucenc->stogy);
        str_free(nucenc->mod);
        str_free(nucenc->trail);
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
    nucenc->first = false;
    str_clear(nucenc->rname_prev);
    str_clear(nucenc->ctrl);
    str_clear(nucenc->poff);
    str_clear(nucenc->stogy);
    str_clear(nucenc->mod);
    str_clear(nucenc->trail);
    cbufint64_clear(nucenc->neo_cbuf);
    cbufint64_clear(nucenc->pos_cbuf);
    cbufstr_clear(nucenc->exs_cbuf);
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
    nucenc->record_cnt++;

    //
    // Sanity check:
    // - Write mRNAME:POS:CIGAR:SEQ~ to CTRL in case of corrupted record
    // - Do -not- push to circular buffer
    //

    if (   (!strlen(rname) || (rname[0] == '*' && rname[1] == '\0'))
        || (!pos)
        || (!strlen(cigar) || (cigar[0] == '*' && cigar[1] == '\0'))
        || (!strlen(seq) || (seq[0] == '*' && seq[1] == '\0'))) {
        DEBUG("Missing RNAME|POS|CIGAR|SEQ\n");

        str_append_cstr(nucenc->ctrl, "m");
        str_append_cstr(nucenc->ctrl, rname);
        str_append_cstr(nucenc->ctrl, ":");
        str_append_int(nucenc->ctrl, pos);
        str_append_cstr(nucenc->ctrl, ":");
        str_append_cstr(nucenc->ctrl, cigar);
        str_append_cstr(nucenc->ctrl, ":");
        str_append_cstr(nucenc->ctrl, seq);
        str_append_cstr(nucenc->ctrl, "~");

        return;
    }

    uint32_t neo = 0;

    //
    // First record in block:
    // - Store RNAME
    // - Expand SEQ using CIGAR
    // - Write fRNAME:POS:STOGY:EXS~ to CTRL
    // - Push NEO, POS, EXS to circular buffer
    //

    if (!nucenc->first) {
        str_t *stogy = str_new();
        str_t *exs = str_new();

        str_copy_cstr(nucenc->rname_prev, rname);

        nucenc_expand(stogy, exs, cigar, seq);
        neo = ceil(stogy->len);

        str_append_cstr(nucenc->ctrl, "f");
        str_append_cstr(nucenc->ctrl, rname);
        str_append_cstr(nucenc->ctrl, ":");
        str_append_int(nucenc->ctrl, pos);
        str_append_cstr(nucenc->ctrl, ":");
        str_append_str(nucenc->ctrl, stogy);
        str_append_cstr(nucenc->ctrl, ":");
        str_append_str(nucenc->ctrl, exs);
        str_append_cstr(nucenc->ctrl, "~");

        cbufint64_push(nucenc->neo_cbuf, neo);
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
    neo = ceil(stogy->len);

    // Get matching NEO, POS, EXS from circular buffer
    size_t cbuf_idx = 0;
    size_t cbuf_n = nucenc->neo_cbuf->n;
    size_t cbuf_idx_best = cbuf_idx;
    uint32_t neo_best = UINT32_MAX;

    do {
        uint32_t neo_ref = (uint32_t)cbufint64_get(nucenc->neo_cbuf, cbuf_idx);
        uint32_t neo_off = abs(100*(1-ALPHA)*(neo-neo_ref) + 100*ALPHA*(cbuf_idx));

        if (neo_off < neo_best) {
            neo_best = neo_off;
            cbuf_idx_best = cbuf_idx;
        }
        cbuf_idx++;
    } while (cbuf_idx < cbuf_n);

    int64_t pos_ref = cbufint64_get(nucenc->pos_cbuf, cbuf_idx_best);
    str_t *exs_ref = cbufstr_get(nucenc->exs_cbuf, cbuf_idx_best);

    // Compute and check position offset
    int64_t pos_off = pos - pos_ref;

    if (   pos_off > exs_ref->len
        || pos_off < 0
        || strcmp(rname, nucenc->rname_prev->s)) {

        //
        // Position offset is negative or too large, or RNAME has changed:
        // - Store RNAME
        // - Write iRNAME:POS:STOGY:EXS to CTRL
        // - Clear circular buffer
        // - Push NEO, POS, EXS to circular buffer
        //

        str_copy_cstr(nucenc->rname_prev, rname);

        str_append_cstr(nucenc->ctrl, "i");
        str_append_cstr(nucenc->ctrl, rname);
        str_append_cstr(nucenc->ctrl, ":");
        str_append_int(nucenc->ctrl, pos);
        str_append_cstr(nucenc->ctrl, ":");
        str_append_str(nucenc->ctrl, stogy);
        str_append_cstr(nucenc->ctrl, ":");
        str_append_str(nucenc->ctrl, exs);
        str_append_cstr(nucenc->ctrl, "~");

        cbufint64_clear(nucenc->neo_cbuf);
        cbufint64_clear(nucenc->pos_cbuf);
        cbufstr_clear(nucenc->exs_cbuf);

        cbufint64_push(nucenc->neo_cbuf, neo);
        cbufint64_push(nucenc->pos_cbuf, pos);
        cbufstr_push(nucenc->exs_cbuf, exs->s);
    } else {

        //
        // "Normal" record
        // - Compute MOD and TRAIL
        // - Write POFF, STOGY, MOD, and TRAIL
        // - Push NEO, POS, and EXS to circular buffer
        //

        nucenc_diff(mod, trail, pos_off, exs->s, exs_ref->s);
        neo = ceil(stogy->len + mod->len);

        str_append_int(nucenc->poff, pos_off);
        str_append_cstr(nucenc->poff, ":");
        str_append_str(nucenc->stogy, stogy);
        str_append_cstr(nucenc->stogy, ":");
        if (mod->len) str_append_str(nucenc->mod, mod);
        str_append_cstr(nucenc->mod, ":");
        if (trail->len) str_append_str(nucenc->trail, trail);
        str_append_cstr(nucenc->trail, ":");
        str_append_cstr(nucenc->ctrl, "~");

        cbufint64_push(nucenc->neo_cbuf, neo);
        cbufint64_push(nucenc->pos_cbuf, pos);
        cbufstr_push(nucenc->exs_cbuf, exs->s);
    }

    str_free(stogy);
    str_free(mod);
    str_free(trail);
    str_free(exs);
}

size_t nucenc_write_block(nucenc_t *nucenc, FILE *fp)
{
    size_t ret = 0;

    //
    // Compress sub-blocks and compute CRCs
    //

    unsigned char *ctrl = (unsigned char *)nucenc->ctrl->s;
    size_t ctrl_sz = nucenc->ctrl->len;
    size_t ctrl_compressed_sz;
    unsigned char *ctrl_compressed = zlib_compress(ctrl, ctrl_sz, &ctrl_compressed_sz);

    unsigned char *poff = (unsigned char *)nucenc->poff->s;
    size_t poff_sz = nucenc->poff->len;
    size_t poff_compressed_sz;
    unsigned char *poff_compressed = zlib_compress(poff, poff_sz, &poff_compressed_sz);

    unsigned char *stogy = (unsigned char *)nucenc->stogy->s;
    size_t stogy_sz = nucenc->stogy->len;
    size_t stogy_compressed_sz;
    unsigned char *stogy_compressed = zlib_compress(stogy, stogy_sz, &stogy_compressed_sz);

    unsigned char *mod = (unsigned char *)nucenc->mod->s;
    size_t mod_sz = nucenc->mod->len;
    size_t mod_compressed_sz;
    unsigned char *mod_compressed = zlib_compress(mod, mod_sz, &mod_compressed_sz);

    unsigned char *trail = (unsigned char *)nucenc->trail->s;
    size_t trail_sz = nucenc->trail->len;
    size_t trail_compressed_sz;
    unsigned char *trail_compressed = zlib_compress(trail, trail_sz, &trail_compressed_sz);

    //
    // Compute CRCs for sub-blocks
    //

    uint64_t ctrl_compressed_crc = crc64(ctrl_compressed, ctrl_compressed_sz);
    uint64_t poff_compressed_crc = crc64(poff_compressed, poff_compressed_sz);
    uint64_t stogy_compressed_crc = crc64(stogy_compressed, stogy_compressed_sz);
    uint64_t mod_compressed_crc = crc64(mod_compressed, mod_compressed_sz);
    uint64_t trail_compressed_crc = crc64(trail_compressed, trail_compressed_sz);

    //
    // Write block header
    //

    unsigned char id[8] = "nuc----"; id[7] = '\0';
    ret += tnt_fwrite_buf(fp, id, sizeof(id));
    ret += tnt_fwrite_uint64(fp, (uint64_t)nucenc->record_cnt);

    //
    // Write compressed sub-blocks
    //

    ret += tnt_fwrite_uint64(fp, (uint64_t)ctrl_sz);
    ret += tnt_fwrite_uint64(fp, (uint64_t)ctrl_compressed_sz);
    ret += tnt_fwrite_uint64(fp, (uint64_t)ctrl_compressed_crc);
    ret += tnt_fwrite_buf(fp, ctrl_compressed, ctrl_compressed_sz);

    ret += tnt_fwrite_uint64(fp, (uint64_t)poff_sz);
    ret += tnt_fwrite_uint64(fp, (uint64_t)poff_compressed_sz);
    ret += tnt_fwrite_uint64(fp, (uint64_t)poff_compressed_crc);
    ret += tnt_fwrite_buf(fp, poff_compressed, poff_compressed_sz);

    ret += tnt_fwrite_uint64(fp, (uint64_t)stogy_sz);
    ret += tnt_fwrite_uint64(fp, (uint64_t)stogy_compressed_sz);
    ret += tnt_fwrite_uint64(fp, (uint64_t)stogy_compressed_crc);
    ret += tnt_fwrite_buf(fp, stogy_compressed, stogy_compressed_sz);

    ret += tnt_fwrite_uint64(fp, (uint64_t)mod_sz);
    ret += tnt_fwrite_uint64(fp, (uint64_t)mod_compressed_sz);
    ret += tnt_fwrite_uint64(fp, (uint64_t)mod_compressed_crc);
    ret += tnt_fwrite_buf(fp, mod_compressed, mod_compressed_sz);

    ret += tnt_fwrite_uint64(fp, (uint64_t)trail_sz);
    ret += tnt_fwrite_uint64(fp, (uint64_t)trail_compressed_sz);
    ret += tnt_fwrite_uint64(fp, (uint64_t)trail_compressed_crc);
    ret += tnt_fwrite_buf(fp, trail_compressed, trail_compressed_sz);

    //
    // Free memory used for encoded bitstreams
    //

    free(ctrl_compressed);
    free(poff_compressed);
    free(stogy_compressed);
    free(mod_compressed);
    free(trail_compressed);

    nucenc_reset(nucenc);
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
    nucdec_t *nucdec = (nucdec_t *)tnt_malloc(sizeof(nucdec_t));
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

static void nucdec_decode_records(nucdec_t      *nucdec,
                                  unsigned char *ctrl,
                                  size_t        ctrl_sz,
                                  unsigned char *poff,
                                  size_t        poff_sz,
                                  unsigned char *stogy,
                                  size_t        stogy_sz,
                                  unsigned char *mod,
                                  size_t        mod_sz,
                                  unsigned char *trail,
                                  size_t        trail_sz,
                                  str_t         **rname,
                                  uint32_t      *pos,
                                  str_t         **cigar,
                                  str_t         **seq)
{
    size_t record_cnt = 0;
    str_t *rname_prev = str_new();

    while (1) {

        // Break if end of CTRL is reached
        if (*ctrl == '\0') break;

        // Make readable pointer aliases for current record placeholders
        str_t *_rname_ = rname[record_cnt];
        uint32_t *_pos_ = &pos[record_cnt];
        str_t *_cigar_ = cigar[record_cnt];
        str_t *_seq_ = seq[record_cnt];

        // Clear current record placeholders (just if the caller hasn't done)
        str_clear(_rname_);
        *_pos_ = 0;
        str_clear(_cigar_);
        str_clear(_seq_);

        // Get CTRL
        str_t *ctrl_curr = str_new();
        while (*ctrl != '~') str_append_char(ctrl_curr, *ctrl++);
        str_append_char(ctrl_curr, *ctrl++);

        if (ctrl_curr->s[0] == 'm') {

            //
            // Skipped record of type mRNAME:POS:CIGAR:SEQ~
            //

            int itr = 1; // Skip 'm'

            // Get RNAME
            while (ctrl_curr->s[itr] != ':' && ctrl_curr->s[itr] != '~') {
                str_append_char(_rname_, ctrl_curr->s[itr]);
                ++itr;
            }
            if (!_rname_->len) str_append_char(_rname_, '*');

            // Get POS
            itr++;
            while (ctrl_curr->s[itr] != ':') {
                *_pos_ = *_pos_ * 10 + ctrl_curr->s[itr] - '0';
                ++itr;
            }

            // Get CIGAR
            itr++;
            while (ctrl_curr->s[itr] != ':') {
                str_append_char(_cigar_, ctrl_curr->s[itr]);
                ++itr;
            }
            if (!_cigar_->len) str_append_char(_cigar_, '*');

            // Get SEQ
            itr++;
            while (ctrl_curr->s[itr] != '~') {
                str_append_char(_seq_, ctrl_curr->s[itr]);
                ++itr;
            }
            if (!_seq_->len) str_append_char(_seq_, '*');

        } else if (ctrl_curr->s[0] == 'f' || ctrl_curr->s[0] == 'i') {

            //
            // First record of type fRNAME:POS:STOGY:EXS~ or inserted I-Record
            // of type iRNAME:POS:STOGY:EXS~
            //

            str_t *stogy_curr = str_new();
            str_t *exs_curr = str_new();

            int itr = 1; // Skip f/i

            // Get RNAME
            while (ctrl_curr->s[itr] != ':')
                str_append_char(_rname_, ctrl_curr->s[itr++]);

            // Get POS
            ++itr;
            while (ctrl_curr->s[itr] != ':')
                *_pos_ = *_pos_ * 10 + ctrl_curr->s[itr++] - '0';

            // Get STOGY
            itr++;
            while (ctrl_curr->s[itr] != ':')
                str_append_char(stogy_curr, ctrl_curr->s[itr++]);

            // Get EXS
            itr++;
            while (ctrl_curr->s[itr] != '~')
                str_append_char(exs_curr, ctrl_curr->s[itr++]);

            // Clear circular buffers
            cbufint64_clear(nucdec->neo_cbuf);
            cbufint64_clear(nucdec->pos_cbuf);
            cbufstr_clear(nucdec->exs_cbuf);

            // Push NEO, POS, EXS to circular buffer
            uint32_t neo = (uint32_t)ceil(stogy_curr->len);
            cbufint64_push(nucdec->neo_cbuf, (int64_t)neo);
            cbufint64_push(nucdec->pos_cbuf, *_pos_);
            cbufstr_push(nucdec->exs_cbuf, exs_curr->s);

            // Contract
            nucdec_contract(_cigar_, _seq_, stogy_curr->s, exs_curr->s);

            // Store RNAME
            str_copy_str(rname_prev, _rname_);

            str_free(stogy_curr);
            str_free(exs_curr);

        } else if (ctrl_curr->s[0] == '~') {

            //
            // This is a "normal" record of type POFF:STOGY[:MOD][:TRAIL]~
            //

            int64_t poff_curr = 0;
            str_t *stogy_curr = str_new();
            str_t *exs_curr = str_new();
            str_t *mod_curr = str_new();
            str_t *trail_curr = str_new();

            // Get POFF
            while (*poff != ':') poff_curr = poff_curr * 10 + *poff++ - '0';
            poff++;

            // Get RNAME
            str_copy_str(_rname_, rname_prev);

            // Get STOGY
            while (*stogy !=':') str_append_char(stogy_curr, *stogy++);
            stogy++;

            // Get MOD
            while (*mod !=':') str_append_char(mod_curr, *mod++);
            mod++;

            // Get TRAIL
            while (*trail !=':')
                str_append_char(trail_curr, *trail++);
            trail++;

            // Compute NEO
            uint32_t neo = ceil(stogy_curr->len);

            // Get matching NEO, POS, EXS from circular buffer
            size_t cbuf_idx = 0;
            size_t cbuf_n = nucdec->pos_cbuf->n;
            size_t cbuf_idx_best = cbuf_idx;
            uint32_t neo_best = UINT32_MAX;

            do {
                uint32_t neo_ref = (uint32_t)cbufint64_get(nucdec->neo_cbuf, cbuf_idx);
                uint32_t neo_off = abs(100*(1-ALPHA)*(neo-neo_ref) + 100*ALPHA*(cbuf_idx));

                if (neo_off < neo_best) {
                    neo_best = neo_off;
                    cbuf_idx_best = cbuf_idx;
                }
                cbuf_idx++;
            } while (cbuf_idx < cbuf_n);

            int64_t pos_ref = cbufint64_get(nucdec->pos_cbuf, cbuf_idx_best);
            str_t *exs_ref = cbufstr_get(nucdec->exs_cbuf, cbuf_idx_best);

            // Compute POS
            *_pos_ = poff_curr + pos_ref;

            // Reintegrate MOD and TRAIL into EXS
            nucdec_alike(exs_curr, exs_ref->s, poff_curr, stogy_curr->s, mod_curr->s, trail_curr->s);

            // Push NEO, POS, EXS to circular buffer
            neo = ceil(stogy_curr->len + mod_curr->len);
            cbufint64_push(nucdec->neo_cbuf, neo);
            cbufint64_push(nucdec->pos_cbuf, *_pos_);
            cbufstr_push(nucdec->exs_cbuf, exs_curr->s);

            // Contract EXS
            nucdec_contract(_cigar_, _seq_, stogy_curr->s, exs_curr->s);

            str_free(stogy_curr);
            str_free(exs_curr);
            str_free(mod_curr);
            str_free(trail_curr);

        } else {
            tsc_error("Invalid tsc record\n");
        }

        record_cnt++;
        str_free(ctrl_curr);
    }

    str_free(rname_prev);
}

size_t nucdec_decode_block(nucdec_t *nucdec,
                           FILE     *fp,
                           str_t    **rname,
                           uint32_t *pos,
                           str_t    **cigar,
                           str_t    **seq)
{
    size_t ret = 0;

    //
    // Read block header
    //

    unsigned char blk_id[8];
    uint64_t record_cnt;
    ret += tnt_fread_buf(fp, blk_id, sizeof(blk_id));
    ret += tnt_fread_uint64(fp, &record_cnt);

    //
    // Read compressed sub-blocks
    //

    uint64_t ctrl_sz;
    uint64_t ctrl_compressed_sz;
    uint64_t ctrl_compressed_crc;
    unsigned char *ctrl_compressed;
    ret += tnt_fread_uint64(fp, &ctrl_sz);
    ret += tnt_fread_uint64(fp, &ctrl_compressed_sz);
    ret += tnt_fread_uint64(fp, &ctrl_compressed_crc);
    ctrl_compressed = (unsigned char *)tnt_malloc((size_t)ctrl_compressed_sz);
    ret += tnt_fread_buf(fp, ctrl_compressed, ctrl_compressed_sz);

    uint64_t poff_sz;
    uint64_t poff_compressed_sz;
    uint64_t poff_compressed_crc;
    unsigned char *poff_compressed;
    ret += tnt_fread_uint64(fp, &poff_sz);
    ret += tnt_fread_uint64(fp, &poff_compressed_sz);
    ret += tnt_fread_uint64(fp, &poff_compressed_crc);
    poff_compressed = (unsigned char *)tnt_malloc((size_t)poff_compressed_sz);
    ret += tnt_fread_buf(fp, poff_compressed, poff_compressed_sz);

    uint64_t stogy_sz;
    uint64_t stogy_compressed_sz;
    uint64_t stogy_compressed_crc;
    unsigned char *stogy_compressed;
    ret += tnt_fread_uint64(fp, &stogy_sz);
    ret += tnt_fread_uint64(fp, &stogy_compressed_sz);
    ret += tnt_fread_uint64(fp, &stogy_compressed_crc);
    stogy_compressed = (unsigned char *)tnt_malloc((size_t)stogy_compressed_sz);
    ret += tnt_fread_buf(fp, stogy_compressed, stogy_compressed_sz);

    uint64_t mod_sz;
    uint64_t mod_compressed_sz;
    uint64_t mod_compressed_crc;
    unsigned char *mod_compressed;
    ret += tnt_fread_uint64(fp, &mod_sz);
    ret += tnt_fread_uint64(fp, &mod_compressed_sz);
    ret += tnt_fread_uint64(fp, &mod_compressed_crc);
    mod_compressed = (unsigned char *)tnt_malloc((size_t)mod_compressed_sz);
    ret += tnt_fread_buf(fp, mod_compressed, mod_compressed_sz);

    uint64_t trail_sz;
    uint64_t trail_compressed_sz;
    uint64_t trail_compressed_crc;
    unsigned char *trail_compressed;
    ret += tnt_fread_uint64(fp, &trail_sz);
    ret += tnt_fread_uint64(fp, &trail_compressed_sz);
    ret += tnt_fread_uint64(fp, &trail_compressed_crc);
    trail_compressed = (unsigned char *)tnt_malloc((size_t)trail_compressed_sz);
    ret += tnt_fread_buf(fp, trail_compressed, trail_compressed_sz);

    //
    // CRC check
    //

    if (crc64(ctrl_compressed, ctrl_compressed_sz) != ctrl_compressed_crc)
        tsc_error("CRC64 check failed for nuc-ctrl block\n");
    if (crc64(ctrl_compressed, ctrl_compressed_sz) != ctrl_compressed_crc)
        tsc_error("CRC64 check failed for nuc-poff block\n");
    if (crc64(stogy_compressed, stogy_compressed_sz) != stogy_compressed_crc)
        tsc_error("CRC64 check failed for nuc-stogy block\n");
    if (crc64(mod_compressed, mod_compressed_sz) != mod_compressed_crc)
        tsc_error("CRC64 check failed for nuc-mod block\n");
    if (crc64(trail_compressed, trail_compressed_sz) != trail_compressed_crc)
        tsc_error("CRC64 check failed for nuc-trail block\n");

    //
    // Decompress sub-blocks
    //

    unsigned char *ctrl = zlib_decompress(ctrl_compressed, ctrl_compressed_sz, ctrl_sz);
    free(ctrl_compressed);
    unsigned char *poff = zlib_decompress(poff_compressed, poff_compressed_sz, poff_sz);
    free(poff_compressed);
    unsigned char *stogy = zlib_decompress(stogy_compressed, stogy_compressed_sz, stogy_sz);
    free(stogy_compressed);
    unsigned char *mod = zlib_decompress(mod_compressed, mod_compressed_sz, mod_sz);
    free(mod_compressed);
    unsigned char *trail = zlib_decompress(trail_compressed, trail_compressed_sz, trail_sz);
    free(trail_compressed);

    //
    // Decode block
    //

    // Terminate buffers
    ctrl = tnt_realloc(ctrl, ++ctrl_sz); ctrl[ctrl_sz-1] = '\0';
    poff = tnt_realloc(poff, ++poff_sz); poff[poff_sz-1] = '\0';
    stogy = tnt_realloc(stogy, ++stogy_sz); stogy[stogy_sz-1] = '\0';
    mod = tnt_realloc(mod, ++mod_sz); mod[mod_sz-1] = '\0';
    trail = tnt_realloc(trail, ++trail_sz); trail[trail_sz-1] = '\0';

    nucdec_decode_records(nucdec,
                          ctrl, ctrl_sz,
                          poff, poff_sz,
                          stogy, stogy_sz,
                          mod, mod_sz,
                          trail, trail_sz,
                          rname, pos, cigar, seq);

    //
    // Free memory used for decoded bitstreams
    //

    free(ctrl);
    free(poff);
    free(stogy);
    free(mod);
    free(trail);

    nucdec_reset(nucdec);
    return ret;
}

