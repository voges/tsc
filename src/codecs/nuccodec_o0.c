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
// Nuc o0 block format:
//   unsigned char  blk_id[8] : "nuc----" + '\0'
//   uint64_t       rec_cnt  : No. of lines in block
//   uint64_t       data_sz  : Data size
//   uint64_t       data_crc : CRC64 of compressed data
//   unsigned char  *data    : Data
//

#include "nuccodec_o0.h"
#include "../range/range.h"
#include "../tsclib.h"
#include "../tvclib/crc64.h"
#include "../tvclib/frw.h"
#include <inttypes.h>
#include <string.h>

// Encoder
// -----------------------------------------------------------------------------

static void nucenc_init(nucenc_t *nucenc)
{
    nucenc->in_sz = 0;
    nucenc->rec_cnt = 0;
}

nucenc_t * nucenc_new(void)
{
    nucenc_t* nucenc = (nucenc_t *)tsc_malloc(sizeof(nucenc_t));
    nucenc->tmp = str_new();
    nucenc_init(nucenc);
    return nucenc;
}

void nucenc_free(nucenc_t *nucenc)
{
    if (nucenc != NULL) {
        str_free(nucenc->tmp);
        free(nucenc);
        nucenc = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

static void nucenc_reset(nucenc_t *nucenc)
{
    str_clear(nucenc->tmp);
    nucenc_init(nucenc);
}

void nucenc_add_record(nucenc_t       *nucenc,
                       const uint32_t pos,
                       const char     *cigar,
                       const char     *seq)
{
    nucenc->in_sz += sizeof(pos);
    nucenc->in_sz += strlen(cigar);
    nucenc->in_sz += strlen(seq);

    nucenc->rec_cnt++;

    char pos_cstr[101];
    snprintf(pos_cstr, sizeof(pos_cstr), "%"PRIu32, pos);

    str_append_cstr(nucenc->tmp, pos_cstr);
    str_append_cstr(nucenc->tmp, "\t");
    str_append_cstr(nucenc->tmp, cigar);
    str_append_cstr(nucenc->tmp, "\t");
    str_append_cstr(nucenc->tmp, seq);
    str_append_cstr(nucenc->tmp, "\n");
}

size_t nucenc_write_block(nucenc_t *nucenc, FILE *fp)
{
    size_t ret = 0;

    // Compress block
    unsigned char *tmp = (unsigned char *)nucenc->tmp->s;
    unsigned int tmp_sz = (unsigned int)nucenc->tmp->len;
    unsigned int data_sz = 0;
    unsigned char *data = range_compress_o0(tmp, tmp_sz, &data_sz);

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
    size_t ret = 0;
    size_t i = 0;
    size_t line = 0;
    unsigned char *cstr = tmp;
    unsigned int idx = 0;

    for (i = 0; i < tmp_sz; i++) {
        if (tmp[i] == '\n') {
            tmp[i] = '\0';
            str_append_cstr(seq[line], (const char *)cstr);
            ret += strlen((const char *)cstr);
            cstr = &tmp[i+1];
            idx = 0;
            line++;
            continue;
        }

        if (tmp[i] == '\t') {
            tmp[i] = '\0';
            switch (idx++) {
            case 0:
                pos[line] = strtoll((const char *)cstr, NULL, 10);
                ret += sizeof(*pos);
                break;
            case 1:
                str_append_cstr(cigar[line], (const char *)cstr);
                ret += strlen((const char *)cstr);
                break;
            default:
                tsc_error("Failed to decode nuc block\n");
                break;
            }
            cstr = &tmp[i+1];
        }
    }

    return ret;
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
    unsigned char * tmp = range_decompress_o0(data, data_sz, &tmp_sz);
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

