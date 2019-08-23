//
// Nuc o1 block format:
//   unsigned char id[8]     : "nuc----" + '\0'
//   uint64_t      record_cnt: No. of lines in block
//   CTRL (zlib block)
//     uint64_t      sz            : Size of uncompressed data
//     uint64_t      compressed_sz : Compressed data size
//     uint64_t      compressed_crc: CRC64 of compressed data
//     unsigned char *compressed   : Compressed data
//   RNAME (zlib block)
//   POS (zlib block)
//   SEQ (zlib block)
//   SEQLEN (rangeO1 block)
//     uint64_t      compressed_sz : Compressed data size
//     uint64_t      compressed_crc: CRC64 of compressed data
//     unsigned char *compressed   : Compressed data
//   EXS (zlib block)
//   POSOFF (rangeO1 block)
//   STOGY (zlib block)
//   INSERTS (zlib block)
//   MODCNT (rangeO1 block)
//   MODPOS (rangeO1 block)
//   MODBASES (zlib block)
//   TRAIL (zlib block)
//

#include "nuccodec.h"
#include <ctype.h>
#include <inttypes.h>
#include <string.h>
#include "crc64.h"
#include "fio.h"
#include "log.h"
#include "mem.h"
#include "range.h"
#include "tsc.h"
#include "zlib-wrap.h"

static void reset_encoder(nuccodec_t *nuccodec);

void reset_encoder(nuccodec_t *nuccodec) {
    nuccodec->record_cnt = 0;
    nuccodec->first = true;
    str_clear(nuccodec->rname_prev);
    nuccodec->pos_prev = 0;
}

static void reset_statistics(nuccodec_t *nuccodec) {
    nuccodec->mrecord_cnt = 0;
    nuccodec->irecord_cnt = 0;
    nuccodec->precord_cnt = 0;
    nuccodec->ctrl_sz = 0;
    nuccodec->rname_sz = 0;
    nuccodec->pos_sz = 0;
    nuccodec->seq_sz = 0;
    nuccodec->seqlen_sz = 0;
    nuccodec->exs_sz = 0;
    nuccodec->posoff_sz = 0;
    nuccodec->stogy_sz = 0;
    nuccodec->inserts_sz = 0;
    nuccodec->modcnt_sz = 0;
    nuccodec->modpos_sz = 0;
    nuccodec->modbases_sz = 0;
    nuccodec->trail_sz = 0;
}

static void reset_streams(nuccodec_t *nuccodec) {
    str_clear(nuccodec->ctrl);
    str_clear(nuccodec->rname);
    str_clear(nuccodec->pos);
    str_clear(nuccodec->seq);
    str_clear(nuccodec->seqlen);
    str_clear(nuccodec->exs);
    str_clear(nuccodec->posoff);
    str_clear(nuccodec->stogy);
    str_clear(nuccodec->inserts);
    str_clear(nuccodec->modcnt);
    str_clear(nuccodec->modpos);
    str_clear(nuccodec->modbases);
    str_clear(nuccodec->trail);
}

static void reset_sliding_window(nuccodec_t *nuccodec) {
    cbufint64_clear(nuccodec->pos_cbuf);
    cbufstr_clear(nuccodec->exs_cbuf);
    str_clear(nuccodec->ref);
    nuccodec->ref_pos_min = 0;
    nuccodec->ref_pos_max = 0;
}

static void init(nuccodec_t *nuccodec) {
    reset_encoder(nuccodec);
    reset_statistics(nuccodec);
    reset_streams(nuccodec);
    reset_sliding_window(nuccodec);
}

static void reset(nuccodec_t *nuccodec) {
    reset_encoder(nuccodec);
    reset_streams(nuccodec);
    reset_sliding_window(nuccodec);
}

nuccodec_t *nuccodec_new(void) {
    nuccodec_t *nuccodec = (nuccodec_t *)tsc_malloc(sizeof(nuccodec_t));

    nuccodec->rname_prev = str_new();
    nuccodec->ctrl = str_new();
    nuccodec->rname = str_new();
    nuccodec->pos = str_new();
    nuccodec->seq = str_new();
    nuccodec->seqlen = str_new();
    nuccodec->exs = str_new();
    nuccodec->posoff = str_new();
    nuccodec->stogy = str_new();
    nuccodec->inserts = str_new();
    nuccodec->modcnt = str_new();
    nuccodec->modpos = str_new();
    nuccodec->modbases = str_new();
    nuccodec->trail = str_new();
    nuccodec->pos_cbuf = cbufint64_new(TSC_NUCCODEC_WINDOW_SIZE);
    nuccodec->exs_cbuf = cbufstr_new(TSC_NUCCODEC_WINDOW_SIZE);
    nuccodec->ref = str_new();

    init(nuccodec);

    return nuccodec;
}

static void print_stats(nuccodec_t *nuccodec) {
    size_t total_record_cnt =
        nuccodec->mrecord_cnt + nuccodec->irecord_cnt + nuccodec->precord_cnt;
    size_t total_sz =
        nuccodec->ctrl_sz + nuccodec->rname_sz + nuccodec->pos_sz +
        nuccodec->seq_sz + nuccodec->exs_sz + nuccodec->posoff_sz +
        nuccodec->stogy_sz + nuccodec->inserts_sz + nuccodec->modcnt_sz +
        nuccodec->modpos_sz + nuccodec->modbases_sz + nuccodec->trail_sz;

    tsc_log("\nnuccodec stats:\n");
    tsc_log("---------------\n");
    tsc_log("mrecords:    %12ld (%6.2f%%)\n", nuccodec->mrecord_cnt,
            100 * (double)nuccodec->mrecord_cnt / (double)total_record_cnt);
    tsc_log("irecords:    %12ld (%6.2f%%)\n", nuccodec->irecord_cnt,
            100 * (double)nuccodec->irecord_cnt / (double)total_record_cnt);
    tsc_log("precords:    %12ld (%6.2f%%)\n", nuccodec->precord_cnt,
            100 * (double)nuccodec->precord_cnt / (double)total_record_cnt);
    tsc_log("--\n");
    tsc_log("ctrl_sz:     %12ld (%6.2f%%)\n", nuccodec->ctrl_sz,
            100 * (double)nuccodec->ctrl_sz / (double)total_sz);
    tsc_log("rname_sz:    %12ld (%6.2f%%)\n", nuccodec->rname_sz,
            100 * (double)nuccodec->rname_sz / (double)total_sz);
    tsc_log("pos_sz:      %12ld (%6.2f%%)\n", nuccodec->pos_sz,
            100 * (double)nuccodec->pos_sz / (double)total_sz);
    tsc_log("seq_sz:      %12ld (%6.2f%%)\n", nuccodec->seq_sz,
            100 * (double)nuccodec->seq_sz / (double)total_sz);
    tsc_log("seqlen_sz:   %12ld (%6.2f%%)\n", nuccodec->seqlen_sz,
            100 * (double)nuccodec->seqlen_sz / (double)total_sz);
    tsc_log("exs_sz:      %12ld (%6.2f%%)\n", nuccodec->exs_sz,
            100 * (double)nuccodec->exs_sz / (double)total_sz);
    tsc_log("posoff_sz:   %12ld (%6.2f%%)\n", nuccodec->posoff_sz,
            100 * (double)nuccodec->posoff_sz / (double)total_sz);
    tsc_log("stogy_sz:    %12ld (%6.2f%%)\n", nuccodec->stogy_sz,
            100 * (double)nuccodec->stogy_sz / (double)total_sz);
    tsc_log("inserts_sz:  %12ld (%6.2f%%)\n", nuccodec->inserts_sz,
            100 * (double)nuccodec->inserts_sz / (double)total_sz);
    tsc_log("modcnt_sz:   %12ld (%6.2f%%)\n", nuccodec->modcnt_sz,
            100 * (double)nuccodec->modcnt_sz / (double)total_sz);
    tsc_log("modpos_sz:   %12ld (%6.2f%%)\n", nuccodec->modpos_sz,
            100 * (double)nuccodec->modpos_sz / (double)total_sz);
    tsc_log("modbases_sz: %12ld (%6.2f%%)\n", nuccodec->modbases_sz,
            100 * (double)nuccodec->modbases_sz / (double)total_sz);
    tsc_log("trail_sz:    %12ld (%6.2f%%)\n\n", nuccodec->trail_sz,
            100 * (double)nuccodec->trail_sz / (double)total_sz);
}

void nuccodec_free(nuccodec_t *nuccodec) {
    if (tsc_stats) print_stats(nuccodec);
    if (nuccodec != NULL) {
        str_free(nuccodec->rname_prev);
        str_free(nuccodec->ctrl);
        str_free(nuccodec->rname);
        str_free(nuccodec->pos);
        str_free(nuccodec->seq);
        str_free(nuccodec->seqlen);
        str_free(nuccodec->exs);
        str_free(nuccodec->posoff);
        str_free(nuccodec->stogy);
        str_free(nuccodec->inserts);
        str_free(nuccodec->modcnt);
        str_free(nuccodec->modpos);
        str_free(nuccodec->modbases);
        str_free(nuccodec->trail);
        cbufint64_free(nuccodec->pos_cbuf);
        cbufstr_free(nuccodec->exs_cbuf);
        str_free(nuccodec->ref);

        free(nuccodec);
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

static void update_sliding_window(nuccodec_t *nuccodec, uint32_t pos,
                                  const char *exs) {
    // Push new POS and EXS to sliding window
    cbufint64_push(nuccodec->pos_cbuf, pos);
    cbufstr_push(nuccodec->exs_cbuf, exs);

    // Update min/max positions
    size_t cbuf_idx = 0;
    size_t cbuf_n = nuccodec->pos_cbuf->n;
    nuccodec->ref_pos_min = UINT32_MAX;
    nuccodec->ref_pos_max = 0;
    do {
        uint32_t pos_curr_min =
            (uint32_t)cbufint64_get(nuccodec->pos_cbuf, cbuf_idx);
        str_t *exs_curr = cbufstr_get(nuccodec->exs_cbuf, cbuf_idx);
        uint32_t pos_curr_max = pos_curr_min + (uint32_t)exs_curr->len - 1;
        if (pos_curr_min < nuccodec->ref_pos_min)
            nuccodec->ref_pos_min = pos_curr_min;
        if (pos_curr_max > nuccodec->ref_pos_max)
            nuccodec->ref_pos_max = pos_curr_max;
        cbuf_idx++;
    } while (cbuf_idx < cbuf_n);

    // Allocate frequency table
    size_t width = nuccodec->ref_pos_max - nuccodec->ref_pos_min + 1;
    size_t height = 6;  // Possible symbols are: A, C, G, T, N, ?
    size_t w = 0, h = 0;
    size_t *freq = (size_t *)tsc_malloc(sizeof(size_t) * width * height);
    for (h = 0; h < height; h++) {
        for (w = 0; w < width; w++) freq[w + h * width] = 0;
    }

    // Fill frequency table
    cbuf_idx = 0;
    do {
        uint32_t pos_curr =
            (uint32_t)cbufint64_get(nuccodec->pos_cbuf, cbuf_idx);
        uint32_t pos_off = pos_curr - nuccodec->ref_pos_min;
        str_t *exs_curr = cbufstr_get(nuccodec->exs_cbuf, cbuf_idx);
        for (w = 0; w < exs_curr->len; w++) {
            switch (exs_curr->s[w]) {
                case 'A':
                    freq[pos_off + w + 0 * width]++;
                    break;
                case 'C':
                    freq[pos_off + w + 1 * width]++;
                    break;
                case 'G':
                    freq[pos_off + w + 2 * width]++;
                    break;
                case 'T':
                    freq[pos_off + w + 3 * width]++;
                    break;
                case 'N':
                    freq[pos_off + w + 4 * width]++;
                    break;
                case '?':
                    freq[pos_off + w + 5 * width]++;
                    break;
                default:
                    tsc_error("Unknown symbol in EXS\n");
            }
        }
        cbuf_idx++;
    } while (cbuf_idx < cbuf_n);

    // Compute consensus reference
    str_clear(nuccodec->ref);
    for (w = 0; w < width; w++) {
        size_t freq_curr_max = 0;
        size_t selected = 0;
        for (h = 0; h < height; h++) {
            if (freq[w + h * width] > freq_curr_max) {
                freq_curr_max = freq[w + h * width];
                selected = h;
            }
        }
        switch (selected) {
            case 0:
                str_append_cstr(nuccodec->ref, "A");
                break;
            case 1:
                str_append_cstr(nuccodec->ref, "C");
                break;
            case 2:
                str_append_cstr(nuccodec->ref, "G");
                break;
            case 3:
                str_append_cstr(nuccodec->ref, "T");
                break;
            case 4:
                str_append_cstr(nuccodec->ref, "N");
                break;
            case 5:
                str_append_cstr(nuccodec->ref, "?");
                break;
            default:
                tsc_error("Unknown symbol in frequency table\n");
        }
    }

    free(freq);
}

// Encoder methods
// -----------------------------------------------------------------------------

/**
 *  Iterate trough the CIGAR string and expand SEQ; this yield EXS, STOGY, and
 *  INSERTS
 */
static void expand(str_t *exs, str_t *stogy, str_t *inserts, const char *cigar,
                   const char *seq) {
    size_t cigar_idx = 0;
    size_t cigar_len = strlen(cigar);
    size_t op_len = 0;  // Length of current CIGAR operation
    size_t seq_idx = 0;

    for (cigar_idx = 0; cigar_idx < cigar_len; cigar_idx++) {
        if (isdigit(cigar[cigar_idx])) {
            op_len = op_len * 10 + (size_t)cigar[cigar_idx] - (size_t)'0';
            continue;
        }

        // Copy CIGAR part to STOGY
        str_append_int(stogy, (int64_t)op_len);
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
                // Add inserted part to INSERTS (-not- to EXS)
                str_append_cstrn(inserts, &seq[seq_idx], op_len);
                seq_idx += op_len;
                break;
            case 'D':
            case 'N': {
                // Inflate EXS
                size_t i = 0;
                for (i = 0; i < op_len; i++) {
                    str_append_char(exs, '?');
                }
                break;
            }
            case 'H':
            case 'P': {
                // These have been clipped
                break;
            }
            default:
                tsc_error("Bad CIGAR string: %s\n", cigar);
        }

        op_len = 0;
    }
}

/**
 *  Compute the modifications between the expanded read EXS mapping to POS
 *  and the sliding reference REF.
 *  The number of  modifications is stored in MODCNT, their positions
 *  are written to MODPOS and the actual modified bases are stored in
 *  MODBASES.
 *  The trailing sequence from EXS is stored in TRAIL.
 */
static bool diff(nuccodec_t *nuccodec, size_t *modcnt, str_t *modpos,
                 str_t *modbases, str_t *trail, const uint32_t pos,
                 const char *exs) {
    // Reset things just if the caller hasn't done yet
    *modcnt = 0;
    str_clear(modpos);
    str_clear(modbases);
    str_clear(trail);

    size_t idx_exs = 0;
    size_t idx_ref = pos - nuccodec->ref_pos_min;
    size_t idx_store = 0;
    size_t idx_prev = 0;

    size_t exs_len = strlen(exs);

    while (exs[idx_exs] && nuccodec->ref->s[idx_ref]) {
        if (exs[idx_exs] != nuccodec->ref->s[idx_ref]) {
            idx_store = idx_exs - idx_prev;
            idx_prev = idx_exs;

            (*modcnt)++;
            str_append_char(modpos, (char)((idx_store >> 8) & 0xFF)); // NOLINT(hicpp-signed-bitwise)
            str_append_char(modpos, (char)((idx_store >> 0) & 0xFF)); // NOLINT(hicpp-signed-bitwise)
            str_append_char(modbases, exs[idx_exs]);
        }
        idx_exs++;
        idx_ref++;

        // Return if too many MODs
        if (*modcnt > exs_len / 2) return false;
    }

    // Append trailing sequence to TRAIL
    while (exs[idx_exs]) str_append_char(trail, exs[idx_exs++]);

    return true;
}

void nuccodec_add_record(nuccodec_t *nuccodec,
                         // const uint16_t flag,
                         const char *rname, const uint32_t pos,
                         const char *cigar, const char *seq) {
    nuccodec->record_cnt++;

    str_t *exs = str_new();
    str_t *stogy = str_new();
    str_t *inserts = str_new();
    size_t modcnt = 0;
    str_t *modpos = str_new();
    str_t *modbases = str_new();
    str_t *trail = str_new();

    if ((!strlen(rname) || (rname[0] == '*' && rname[1] == '\0')) || (!pos) ||
        (!strlen(cigar) || (cigar[0] == '*' && cigar[1] == '\0')) ||
        (!strlen(seq) || (seq[0] == '*' && seq[1] == '\0'))
        /*|| (flag & 0x4)*/) {  // Also try to code unmapped reads
        // tsc_log("Missing RNAME|POS|CIGAR|SEQ\n");
        goto add_mrecord;
    }

    // Expand SEQ using CIGAR, this yields EXS, STOGY, and INSERTS
    expand(exs, stogy, inserts, cigar, seq);

    // Check for first read in block
    if (nuccodec->first) {
        nuccodec->first = false;
        goto add_irecord;
    }

    // Compute and check POS and RNAME of the current read
    int64_t pos_off = pos - nuccodec->pos_prev;
    if (pos_off < 0) tsc_error("SAM file not sorted\n");
    if (strcmp(rname, nuccodec->rname_prev->s) != 0) goto add_irecord;
    if (pos > nuccodec->ref_pos_max) goto add_irecord;
    if (pos_off > UINT16_MAX) goto add_irecord;

    goto add_precord;

add_mrecord:;
    nuccodec->mrecord_cnt++;

    str_append_cstr(nuccodec->ctrl, "m");
    str_append_cstr(nuccodec->rname, rname);
    str_append_cstr(nuccodec->rname, ":");
    str_append_int(nuccodec->pos, pos);
    str_append_cstr(nuccodec->pos, ":");
    str_append_cstr(nuccodec->stogy, cigar);
    str_append_cstr(nuccodec->stogy, ":");
    str_append_char(nuccodec->seqlen, (char)(strlen(seq) >> 8 & 0xFF)); // NOLINT(hicpp-signed-bitwise)
    str_append_char(nuccodec->seqlen, (char)(strlen(seq) >> 0 & 0xFF)); // NOLINT(hicpp-signed-bitwise)
    str_append_cstr(nuccodec->seq, seq);

    goto cleanup;

add_irecord:;  // This is the first read in a block
    nuccodec->irecord_cnt++;

    str_append_cstr(nuccodec->ctrl, "i");
    str_append_cstr(nuccodec->rname, rname);
    str_append_cstr(nuccodec->rname, ":");
    str_append_int(nuccodec->pos, pos);
    str_append_cstr(nuccodec->pos, ":");
    str_append_str(nuccodec->exs, exs);
    str_append_str(nuccodec->stogy, stogy);
    str_append_cstr(nuccodec->stogy, ":");
    str_append_str(nuccodec->inserts, inserts);

    reset_sliding_window(nuccodec);
    update_sliding_window(nuccodec, pos, exs->s);

    str_copy_cstr(nuccodec->rname_prev, rname);
    nuccodec->pos_prev = pos;

    goto cleanup;

add_precord:;  // This read passed all checks and can be coded
    nuccodec->precord_cnt++;

    bool success =
        diff(nuccodec, &modcnt, modpos, modbases, trail, pos, exs->s);
    if (!success) goto add_mrecord;
    if (modcnt > UINT16_MAX) goto add_mrecord;

    str_append_cstr(nuccodec->ctrl, "p");
    str_append_char(nuccodec->posoff, (char)((pos_off >> 8) & 0xFF)); // NOLINT(hicpp-signed-bitwise)
    str_append_char(nuccodec->posoff, (char)((pos_off >> 0) & 0xFF)); // NOLINT(hicpp-signed-bitwise)
    str_append_str(nuccodec->stogy, stogy);
    str_append_cstr(nuccodec->stogy, ":");
    str_append_str(nuccodec->inserts, inserts);
    str_append_char(nuccodec->modcnt, (char)((modcnt >> 8) & 0xFF)); // NOLINT(hicpp-signed-bitwise)
    str_append_char(nuccodec->modcnt, (char)((modcnt >> 0) & 0xFF)); // NOLINT(hicpp-signed-bitwise)
    str_append_str(nuccodec->modpos, modpos);
    str_append_str(nuccodec->modbases, modbases);
    str_append_str(nuccodec->trail, trail);

    update_sliding_window(nuccodec, pos, exs->s);

    str_copy_cstr(nuccodec->rname_prev, rname);
    nuccodec->pos_prev = pos;

    goto cleanup;

cleanup:;
    str_free(exs);
    str_free(stogy);
    str_free(inserts);
    str_free(modpos);
    str_free(modbases);
    str_free(trail);
}

static size_t write_zlib_block(FILE *fp, unsigned char *data, size_t data_sz) {
    size_t ret = 0;
    size_t compressed_sz;
    unsigned char *compressed = zlib_compress(data, data_sz, &compressed_sz);
    uint64_t compressed_crc = crc64(compressed, compressed_sz);
    ret += tsc_fwrite_uint64(fp, (uint64_t)data_sz);
    ret += tsc_fwrite_uint64(fp, (uint64_t)compressed_sz);
    ret += tsc_fwrite_uint64(fp, (uint64_t)compressed_crc);
    ret += tsc_fwrite_buf(fp, compressed, compressed_sz);
    free(compressed);
    return ret;
}

static size_t write_rangeO1_block(FILE *fp, unsigned char *data,
                                  size_t data_sz) {
    size_t ret = 0;
    size_t compressed_sz = 0;
    unsigned char *compressed = range_compress_o1(
        data, (unsigned int)data_sz, (unsigned int *)&compressed_sz);
    uint64_t compressed_crc = crc64(compressed, compressed_sz);
    ret += tsc_fwrite_uint64(fp, (uint64_t)compressed_sz);
    ret += tsc_fwrite_uint64(fp, (uint64_t)compressed_crc);
    ret += tsc_fwrite_buf(fp, compressed, compressed_sz);
    free(compressed);
    return ret;
}

size_t nuccodec_write_block(nuccodec_t *nuccodec, FILE *fp) {
    size_t ret = 0;

    // Write block header
    unsigned char id[8] = "nuc----";
    id[7] = '\0';
    ret += tsc_fwrite_buf(fp, id, sizeof(id));
    ret += tsc_fwrite_uint64(fp, (uint64_t)nuccodec->record_cnt);

    // Compress and write sub-blocks
    size_t ctrl_sz = 0;
    size_t rname_sz = 0;
    size_t pos_sz = 0;
    size_t seq_sz = 0;
    size_t seqlen_sz = 0;
    size_t exs_sz = 0;
    size_t posoff_sz = 0;
    size_t stogy_sz = 0;
    size_t inserts_sz = 0;
    size_t modcnt_sz = 0;
    size_t modpos_sz = 0;
    size_t modbases_sz = 0;
    size_t trail_sz = 0;
    ret += ctrl_sz = write_zlib_block(fp, (unsigned char *)nuccodec->ctrl->s,
                                      nuccodec->ctrl->len);
    ret += rname_sz = write_zlib_block(fp, (unsigned char *)nuccodec->rname->s,
                                       nuccodec->rname->len);
    ret += pos_sz = write_zlib_block(fp, (unsigned char *)nuccodec->pos->s,
                                     nuccodec->pos->len);
    ret += seq_sz = write_zlib_block(fp, (unsigned char *)nuccodec->seq->s,
                                     nuccodec->seq->len);
    ret += seqlen_sz = write_rangeO1_block(
        fp, (unsigned char *)nuccodec->seqlen->s, nuccodec->seqlen->len);
    ret += exs_sz = write_zlib_block(fp, (unsigned char *)nuccodec->exs->s,
                                     nuccodec->exs->len);
    ret += posoff_sz = write_rangeO1_block(
        fp, (unsigned char *)nuccodec->posoff->s, nuccodec->posoff->len);
    ret += stogy_sz = write_zlib_block(fp, (unsigned char *)nuccodec->stogy->s,
                                       nuccodec->stogy->len);
    ret += inserts_sz = write_zlib_block(
        fp, (unsigned char *)nuccodec->inserts->s, nuccodec->inserts->len);
    ret += modcnt_sz = write_rangeO1_block(
        fp, (unsigned char *)nuccodec->modcnt->s, nuccodec->modcnt->len);
    ret += modpos_sz = write_rangeO1_block(
        fp, (unsigned char *)nuccodec->modpos->s, nuccodec->modpos->len);
    ret += modbases_sz = write_zlib_block(
        fp, (unsigned char *)nuccodec->modbases->s, nuccodec->modbases->len);
    ret += trail_sz = write_zlib_block(fp, (unsigned char *)nuccodec->trail->s,
                                       nuccodec->trail->len);
    nuccodec->ctrl_sz += ctrl_sz;
    nuccodec->rname_sz += rname_sz;
    nuccodec->pos_sz += pos_sz;
    nuccodec->seq_sz += seq_sz;
    nuccodec->seqlen_sz += seqlen_sz;
    nuccodec->exs_sz += exs_sz;
    nuccodec->posoff_sz += posoff_sz;
    nuccodec->stogy_sz += stogy_sz;
    nuccodec->inserts_sz += inserts_sz;
    nuccodec->modcnt_sz += modcnt_sz;
    nuccodec->modpos_sz += modpos_sz;
    nuccodec->modbases_sz += modbases_sz;
    nuccodec->trail_sz += trail_sz;

    reset(nuccodec);

    return ret;
}

// Decoder methods
// -----------------------------------------------------------------------------

static size_t exslen(const char *cigar) {
    size_t idx = 0;
    size_t op_len = 0;
    size_t ret = 0;
    size_t cigar_len = strlen(cigar);

    for (idx = 0; idx < cigar_len; idx++) {
        if (isdigit(cigar[idx])) {
            op_len = op_len * 10 + (size_t)cigar[idx] - (size_t)'0';
            continue;
        }

        switch (cigar[idx]) {
            case 'M':
            case '=':
            case 'X':
                ret += op_len;
                break;
            case 'I':
            case 'S':
                break;
            case 'D':
            case 'N': {
                ret += op_len;
                break;
            }
            case 'H':
            case 'P': {
                break;
            }
            default:
                tsc_error("Bad CIGAR/STOGY string: %s\n", cigar);
        }

        op_len = 0;
    }

    return ret;
}

static size_t insertslen(const char *cigar) {
    size_t idx = 0;
    size_t op_len = 0;
    size_t ret = 0;
    size_t cigar_len = strlen(cigar);

    for (idx = 0; idx < cigar_len; idx++) {
        if (isdigit(cigar[idx])) {
            op_len = op_len * 10 + (size_t)cigar[idx] - (size_t)'0';
            continue;
        }

        switch (cigar[idx]) {
            case 'M':
            case '=':
            case 'X':
                break;
            case 'I':
            case 'S':
                ret += op_len;
                break;
            case 'D':
            case 'N': {
                break;
            }
            case 'H':
            case 'P': {
                break;
            }
            default:
                tsc_error("Bad CIGAR/STOGY string: %s\n", cigar);
        }

        op_len = 0;
    }

    return ret;
}

/**
 *  Counterpart to expand()
 */
static void contract(str_t *seq, const char *exs, const char *stogy,
                     const char *inserts) {
    size_t stogy_idx = 0;
    size_t inserts_idx = 0;
    size_t stogy_len = strlen(stogy);
    size_t op_len = 0;  // Length of current STOGY operation
    size_t exs_idx = 0;

    for (stogy_idx = 0; stogy_idx < stogy_len; stogy_idx++) {
        if (isdigit(stogy[stogy_idx])) {
            op_len = op_len * 10 + (size_t)stogy[stogy_idx] - (size_t)'0';
            continue;
        }

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
                // Copy inserted part from INSERTS to SEQ and move INSERTS
                // pointer
                str_append_cstrn(seq, &inserts[inserts_idx], op_len);
                inserts_idx += op_len;
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
            default:
                tsc_error("Bad CIGAR/STOGY string\n");
        }

        op_len = 0;
    }
}

/**
 *  Counterpart to diff()
 */
static void alike(str_t *exs, const size_t exs_len, const char *ref,
                  const size_t ref_pos_min, const uint32_t pos,
                  const uint16_t modcnt, const uint16_t *modpos,
                  const char *modbases, const char *trail) {
    // Compute match length
    size_t match_len = 0;
    if (strlen(ref) - (pos - ref_pos_min) <= exs_len)
        match_len = strlen(ref) - (pos - ref_pos_min);
    else
        match_len = exs_len;

    // Copy match from REF
    str_append_cstrn(exs, &ref[pos - ref_pos_min], match_len);

    // Replace MODs
    size_t i = 0;
    size_t modpos_curr = 0;
    size_t modpos_prev = 0;
    for (i = 0; i < modcnt; i++) {
        modpos_curr = modpos_prev + modpos[i];
        modpos_prev = modpos_curr;
        exs->s[modpos_curr] = modbases[i];
    }

    // Append TRAIL
    str_append_cstr(exs, trail);
}

static void nuccodec_decode_records(
    nuccodec_t *nuccodec, unsigned char *ctrl, unsigned char *rname,
    const unsigned char *pos, unsigned char *seq, const unsigned char *seqlen,
    unsigned char *exs, const unsigned char *posoff, unsigned char *stogy,
    unsigned char *inserts, const unsigned char *modcnt,
    const unsigned char *modpos, unsigned char *modbases, unsigned char *trail,
    str_t **rname_decoded, uint32_t *pos_decoded, str_t **cigar_decoded,
    str_t **seq_decoded) {
    size_t record_idx = 0;
    size_t i = 0;

    size_t rname_idx = 0;
    size_t pos_idx = 0;
    size_t seq_idx = 0;
    size_t seqlen_idx = 0;
    size_t exs_idx = 0;
    size_t posoff_idx = 0;
    size_t stogy_idx = 0;
    size_t inserts_idx = 0;
    size_t modcnt_idx = 0;
    size_t modpos_idx = 0;
    size_t modbases_idx = 0;
    size_t trail_idx = 0;

    while (1) {
        if (*ctrl == '\0') break;

        // Make readable pointer aliases for current record placeholders
        str_t *_rname_ = rname_decoded[record_idx];
        uint32_t *_pos_ = &pos_decoded[record_idx];
        str_t *_cigar_ = cigar_decoded[record_idx];
        str_t *_seq_ = seq_decoded[record_idx];

        // Clear current record placeholders if the caller hasn't done yet
        str_clear(_rname_);
        *_pos_ = 0;
        str_clear(_cigar_);
        str_clear(_seq_);

        if (*ctrl == 'm') {
            nuccodec->mrecord_cnt++;

            while (rname[rname_idx] != ':')
                str_append_char(_rname_, (char)rname[rname_idx++]);
            if (!_rname_->len) str_append_char(_rname_, '*');
            rname_idx++;

            while (pos[pos_idx] != ':')
                *_pos_ = *_pos_ * 10 + (uint32_t)(pos[pos_idx++] - '0');
            pos_idx++;

            while (stogy[stogy_idx] != ':')
                str_append_char(_cigar_, (char)stogy[stogy_idx++]);
            if (!_cigar_->len) str_append_char(_cigar_, '*');
            stogy_idx++;

            uint16_t seqlen_curr =
                (uint16_t)((seqlen[seqlen_idx] << 8) + seqlen[seqlen_idx + 1]); // NOLINT(hicpp-signed-bitwise)
            seqlen_idx += 2;

            for (i = 0; i < seqlen_curr; i++)
                str_append_char(_seq_, (char)seq[seq_idx++]);
            if (!_seq_->len) str_append_char(_seq_, '*');
        } else if (*ctrl == 'i') {
            nuccodec->irecord_cnt++;

            while (rname[rname_idx] != ':')
                str_append_char(_rname_, (char)rname[rname_idx++]);
            rname_idx++;

            while (pos[pos_idx] != ':')
                *_pos_ = *_pos_ * 10 + (uint32_t)(pos[pos_idx++] - '0');
            pos_idx++;

            while (stogy[stogy_idx] != ':')
                str_append_char(_cigar_, (char)stogy[stogy_idx++]);
            stogy_idx++;

            size_t exs_len = exslen(_cigar_->s);
            str_t *exs_curr = str_new();
            for (i = 0; i < exs_len; i++)
                str_append_char(exs_curr, (char)exs[exs_idx++]);

            size_t inserts_len = insertslen(_cigar_->s);
            str_t *inserts_curr = str_new();
            for (i = 0; i < inserts_len; i++)
                str_append_char(inserts_curr, (char)inserts[inserts_idx++]);

            contract(_seq_, exs_curr->s, _cigar_->s, inserts_curr->s);

            reset_sliding_window(nuccodec);
            update_sliding_window(nuccodec, *_pos_, exs_curr->s);

            str_free(exs_curr);
            str_free(inserts_curr);

            str_copy_str(nuccodec->rname_prev, _rname_);
            nuccodec->pos_prev = *_pos_;
        } else if (*ctrl == 'p') {
            nuccodec->precord_cnt++;

            str_copy_str(_rname_, nuccodec->rname_prev);

            uint16_t posoff_curr =
                (uint16_t)((posoff[posoff_idx] << 8) | posoff[posoff_idx + 1]); // NOLINT(hicpp-signed-bitwise)
            posoff_idx += 2;

            while (stogy[stogy_idx] != ':')
                str_append_char(_cigar_, (char)stogy[stogy_idx++]);
            stogy_idx++;

            size_t inserts_len = insertslen(_cigar_->s);
            str_t *inserts_curr = str_new();
            for (i = 0; i < inserts_len; i++)
                str_append_char(inserts_curr, (char)inserts[inserts_idx++]);

            uint16_t modcnt_curr =
                (uint16_t)((modcnt[modcnt_idx] << 8) + modcnt[modcnt_idx + 1]); // NOLINT(hicpp-signed-bitwise)
            modcnt_idx += 2;

            uint16_t *modpos_curr =
                (uint16_t *)tsc_malloc(sizeof(uint16_t) * modcnt_curr);
            for (i = 0; i < modcnt_curr; i++) {
                modpos_curr[i] = (uint16_t)((modpos[modpos_idx] << 8) + // NOLINT(hicpp-signed-bitwise)
                                            modpos[modpos_idx + 1]);
                modpos_idx += 2;
            }

            str_t *modbases_curr = str_new();
            for (i = 0; i < modcnt_curr; i++)
                str_append_char(modbases_curr, (char)modbases[modbases_idx++]);

            *_pos_ = nuccodec->pos_prev + posoff_curr;
            size_t exs_len = exslen(_cigar_->s);
            size_t exs_pos_max = *_pos_ + exs_len - 1;
            size_t trail_len = 0;
            if (exs_pos_max <= nuccodec->ref_pos_max)
                trail_len = 0;
            else
                trail_len = exs_pos_max - nuccodec->ref_pos_max;

            str_t *trail_curr = str_new();
            for (i = 0; i < trail_len; i++)
                str_append_char(trail_curr, (char)trail[trail_idx++]);

            str_t *exs_curr = str_new();
            alike(exs_curr, exs_len, nuccodec->ref->s, nuccodec->ref_pos_min,
                  *_pos_, modcnt_curr, modpos_curr, modbases_curr->s,
                  trail_curr->s);

            contract(_seq_, exs_curr->s, _cigar_->s, inserts_curr->s);

            update_sliding_window(nuccodec, *_pos_, exs_curr->s);

            str_copy_str(nuccodec->rname_prev, _rname_);
            nuccodec->pos_prev = *_pos_;

            free(modpos_curr);
            str_free(exs_curr);
            str_free(trail_curr);
            str_free(inserts_curr);
            str_free(modbases_curr);
        } else {
            tsc_error("Invalid ctrl string\n");
        }

        ctrl++;
        record_idx++;
    }
}

static unsigned char *read_zlib_block(FILE *fp, size_t *sz) {
    uint64_t data_sz = 0;
    uint64_t data_compressed_sz = 0;
    uint64_t data_compressed_crc = 0;
    unsigned char *data_compressed;
    *sz += tsc_fread_uint64(fp, &data_sz);
    *sz += tsc_fread_uint64(fp, &data_compressed_sz);
    *sz += tsc_fread_uint64(fp, &data_compressed_crc);
    data_compressed = (unsigned char *)tsc_malloc((size_t)data_compressed_sz);
    *sz += tsc_fread_buf(fp, data_compressed, data_compressed_sz);
    if (crc64(data_compressed, data_compressed_sz) != data_compressed_crc)
        tsc_error("CRC64 check failed\n");
    unsigned char *data =
        zlib_decompress(data_compressed, data_compressed_sz, data_sz);
    free(data_compressed);
    data = tsc_realloc(data, ++data_sz);
    data[data_sz - 1] = '\0';
    return data;
}

static unsigned char *read_rangeO1_block(FILE *fp, size_t *sz) {
    uint64_t data_sz = 0;
    uint64_t data_compressed_sz = 0;
    uint64_t data_compressed_crc = 0;
    unsigned char *data_compressed;
    *sz += tsc_fread_uint64(fp, &data_compressed_sz);
    *sz += tsc_fread_uint64(fp, &data_compressed_crc);
    data_compressed = (unsigned char *)tsc_malloc((size_t)data_compressed_sz);
    *sz += tsc_fread_buf(fp, data_compressed, data_compressed_sz);
    if (crc64(data_compressed, data_compressed_sz) != data_compressed_crc)
        tsc_error("CRC64 check failed\n");
    unsigned char *data =
        range_decompress_o1(data_compressed, (unsigned int *)&data_sz);
    free(data_compressed);
    data = tsc_realloc(data, ++data_sz);
    data[data_sz - 1] = '\0';

    return data;
}

size_t nuccodec_decode_block(nuccodec_t *nuccodec, FILE *fp,
                             str_t **rname_decoded, uint32_t *pos_decoded,
                             str_t **cigar_decoded, str_t **seq_decoded) {
    size_t ret = 0;

    // Read block header
    unsigned char id[8];
    uint64_t record_cnt;
    ret += tsc_fread_buf(fp, id, sizeof(id));
    ret += tsc_fread_uint64(fp, &record_cnt);

    // Read compressed sub-blocks
    size_t ctrl_sz = 0;
    size_t rname_sz = 0;
    size_t pos_sz = 0;
    size_t seq_sz = 0;
    size_t seqlen_sz = 0;
    size_t exs_sz = 0;
    size_t posoff_sz = 0;
    size_t stogy_sz = 0;
    size_t inserts_sz = 0;
    size_t modcnt_sz = 0;
    size_t modpos_sz = 0;
    size_t modbases_sz = 0;
    size_t trail_sz = 0;
    unsigned char *ctrl = read_zlib_block(fp, &ctrl_sz);
    unsigned char *rname = read_zlib_block(fp, &rname_sz);
    unsigned char *pos = read_zlib_block(fp, &pos_sz);
    unsigned char *seq = read_zlib_block(fp, &seq_sz);
    unsigned char *seqlen = read_rangeO1_block(fp, &seqlen_sz);
    unsigned char *exs = read_zlib_block(fp, &exs_sz);
    unsigned char *posoff = read_rangeO1_block(fp, &posoff_sz);
    unsigned char *stogy = read_zlib_block(fp, &stogy_sz);
    unsigned char *inserts = read_zlib_block(fp, &inserts_sz);
    unsigned char *modcnt = read_rangeO1_block(fp, &modcnt_sz);
    unsigned char *modpos = read_rangeO1_block(fp, &modpos_sz);
    unsigned char *modbases = read_zlib_block(fp, &modbases_sz);
    unsigned char *trail = read_zlib_block(fp, &trail_sz);
    nuccodec->ctrl_sz += ctrl_sz;
    nuccodec->rname_sz += rname_sz;
    nuccodec->pos_sz += pos_sz;
    nuccodec->seq_sz += seq_sz;
    nuccodec->seqlen_sz += seqlen_sz;
    nuccodec->exs_sz += exs_sz;
    nuccodec->posoff_sz += posoff_sz;
    nuccodec->stogy_sz += stogy_sz;
    nuccodec->inserts_sz += inserts_sz;
    nuccodec->modcnt_sz += modcnt_sz;
    nuccodec->modpos_sz += modpos_sz;
    nuccodec->modbases_sz += modbases_sz;
    nuccodec->trail_sz += trail_sz;
    ret += ctrl_sz;
    ret += rname_sz;
    ret += pos_sz;
    ret += seq_sz;
    ret += seqlen_sz;
    ret += exs_sz;
    ret += posoff_sz;
    ret += stogy_sz;
    ret += inserts_sz;
    ret += modcnt_sz;
    ret += modpos_sz;
    ret += modbases_sz;
    ret += trail_sz;

    // Decode block
    nuccodec_decode_records(nuccodec, ctrl, rname, pos, seq, seqlen, exs,
                            posoff, stogy, inserts, modcnt, modpos, modbases,
                            trail, rname_decoded, pos_decoded, cigar_decoded,
                            seq_decoded);

    // Free memory used for decoded bitstreams
    free(ctrl);
    free(rname);
    free(pos);
    free(seq);
    free(seqlen);
    free(exs);
    free(posoff);
    free(stogy);
    free(inserts);
    free(modcnt);
    free(modpos);
    free(modbases);
    free(trail);

    reset(nuccodec);

    return ret;
}
