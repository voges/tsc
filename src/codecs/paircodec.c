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
// Pair block format:
//   unsigned char  blk_id[8]: "pair---" + '\0'
//   uint64_t       rec_cnt  : No. of lines in block
//   uint64_t       data_sz  : Data size
//   uint64_t       data_crc : CRC64 of compressed data
//   unsigned char  *data    : Data
//

#include "paircodec.h"
#include "../range.h"
#include "../tsclib.h"
#include "../tvclib/crc64.h"
#include "../tvclib/frw.h"
#include <inttypes.h>
#include <string.h>

// Encoder
// -----------------------------------------------------------------------------

static void pairenc_init(pairenc_t *pairenc)
{
    pairenc->in_sz = 0;
    pairenc->rec_cnt = 0;
}

pairenc_t * pairenc_new(void)
{
    pairenc_t *pairenc = (pairenc_t *)tsc_malloc(sizeof(pairenc_t));
    pairenc->tmp = str_new();
    pairenc_init(pairenc);
    return pairenc;
}

void pairenc_free(pairenc_t *pairenc)
{
    if (pairenc != NULL) {
        str_free(pairenc->tmp);
        free(pairenc);
        pairenc = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

static void pairenc_reset(pairenc_t *pairenc)
{
    str_clear(pairenc->tmp);
    pairenc_init(pairenc);
}

void pairenc_add_record(pairenc_t      *pairenc,
                        const char     *rnext,
                        const uint32_t pnext,
                        const int64_t  tlen)
{
    pairenc->in_sz += strlen(rnext);
    pairenc->in_sz += sizeof(pnext);
    pairenc->in_sz += sizeof(tlen);

    pairenc->rec_cnt++;

    char pnext_cstr[101];
    char tlen_cstr[101];

    snprintf(pnext_cstr, sizeof(pnext_cstr), "%"PRIu32, pnext);
    snprintf(tlen_cstr, sizeof(tlen_cstr), "%"PRId64, tlen);

    str_append_cstr(pairenc->tmp, rnext);
    str_append_cstr(pairenc->tmp, "\t");
    str_append_cstr(pairenc->tmp, pnext_cstr);
    str_append_cstr(pairenc->tmp, "\t");
    str_append_cstr(pairenc->tmp, tlen_cstr);
    str_append_cstr(pairenc->tmp, "\n");
}

size_t pairenc_write_block(pairenc_t *pairenc, FILE *fp)
{
    size_t ret = 0;

    // Compress block
    unsigned char *tmp = (unsigned char *)pairenc->tmp->s;
    unsigned int tmp_sz = (unsigned int)pairenc->tmp->len;
    unsigned int data_sz = 0;
    unsigned char *data = range_compress_o0(tmp, tmp_sz, &data_sz);

    // Compute CRC64
    uint64_t data_crc = crc64(data, data_sz);

    // Write compressed block
    unsigned char blk_id[8] = "pair---"; blk_id[7] = '\0';
    ret += fwrite_buf(fp, blk_id, sizeof(blk_id));
    ret += fwrite_uint64(fp, (uint64_t)pairenc->rec_cnt);
    ret += fwrite_uint64(fp, (uint64_t)data_sz);
    ret += fwrite_uint64(fp, (uint64_t)data_crc);
    ret += fwrite_buf(fp, data, data_sz);

    tsc_vlog("Compressed pair block: %zu bytes -> %zu bytes (%6.2f%%)\n",
             pairenc->in_sz,
             data_sz,
             (double)data_sz / (double)pairenc->in_sz * 100);

    pairenc_reset(pairenc); // Reset encoder for next block
    free(data); // Free memory used for encoded bitstream

    return ret;
}

// Decoder
// -----------------------------------------------------------------------------

static void pairdec_init(pairdec_t *pairdec)
{
    pairdec->out_sz = 0;
}

pairdec_t * pairdec_new(void)
{
    pairdec_t *pairdec = (pairdec_t *)tsc_malloc(sizeof(pairdec_t));
    pairdec_init(pairdec);
    return pairdec;
}

void pairdec_free(pairdec_t *pairdec)
{
    if (pairdec != NULL) {
        free(pairdec);
        pairdec = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

static void pairdec_reset(pairdec_t* pairdec)
{
    pairdec_init(pairdec);
}

static size_t pairdec_decode(pairdec_t     *pairdec,
                             unsigned char *tmp,
                             size_t        tmp_sz,
                             str_t         **rnext,
                             uint32_t      *pnext,
                             int64_t       *tlen)
{
    size_t ret = 0;
    size_t i = 0;
    size_t line = 0;
    unsigned char *cstr = tmp;
    unsigned int idx = 0;

    for (i = 0; i < tmp_sz; i++) {
        if (tmp[i] == '\n') {
            tmp[i] = '\0';
            tlen[line] = strtoll((const char *)cstr, NULL, 10);
            ret += sizeof(*tlen);
            cstr = &tmp[i+1];
            idx = 0;
            line++;
            continue;
        }

        if (tmp[i] == '\t') {
            tmp[i] = '\0';
            switch (idx++) {
            case 0:
                str_append_cstr(rnext[line], (const char *)cstr);
                ret += strlen((const char *)cstr);
                break;
            case 1:
                pnext[line] = strtoll((const char *)cstr, NULL, 10);
                ret += sizeof(*pnext);
                break;
            default:
                tsc_error("Wrong pair bit stream\n");
            }
            cstr = &tmp[i+1];
        }
    }

    return ret;
}

size_t pairdec_decode_block(pairdec_t *pairdec,
                            FILE      *fp,
                            str_t     **rnext,
                            uint32_t  *pnext,
                            int64_t   *tlen)
{
    unsigned char blk_id[8];
    uint64_t      rec_cnt;
    uint64_t      data_sz;
    uint64_t      data_crc;
    unsigned char *data;

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
        tsc_error("CRC64 check failed for pair block\n");

    // Decompress block
    unsigned int tmp_sz = 0;
    unsigned char *tmp = range_decompress_o0(data, data_sz, &tmp_sz);
    free(data);

    // Decode block
    pairdec->out_sz = pairdec_decode(pairdec, tmp, tmp_sz, rnext, pnext, tlen);
    free(tmp); // Free memory used for decoded bitstream

    tsc_vlog("Decompressed pair block: %zu bytes -> %zu bytes (%6.2f%%)\n",
             data_sz,
             pairdec->out_sz,
             (double)pairdec->out_sz / (double)data_sz * 100);

    pairdec_reset(pairdec);

    return ret;
}

