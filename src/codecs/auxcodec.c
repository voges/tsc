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
// Aux block format:
//   unsigned char  blk_id[8]: "aux----" + '\0'
//   uint64_t       rec_cnt  : No. of lines in block
//   uint64_t       data_sz  : Data size
//   uint64_t       data_crc : CRC64 of compressed data
//   unsigned char  *data    : Data
//

#include "auxcodec.h"
#include "../range/range.h"
#include "../tsclib.h"
#include "../tvclib/crc64.h"
#include "../tvclib/frw.h"
#include <inttypes.h>
#include <string.h>

// Encoder
// -----------------------------------------------------------------------------

static void auxenc_init(auxenc_t *auxenc)
{
    auxenc->in_sz = 0;
    auxenc->rec_cnt = 0;
}

auxenc_t * auxenc_new(void)
{
    auxenc_t *auxenc = (auxenc_t *)tsc_malloc(sizeof(auxenc_t));
    auxenc->tmp = str_new();
    auxenc_init(auxenc);
    return auxenc;
}

void auxenc_free(auxenc_t *auxenc)
{
    if (auxenc != NULL) {
        str_free(auxenc->tmp);
        free(auxenc);
        auxenc = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

static void auxenc_reset(auxenc_t *auxenc)
{
    str_clear(auxenc->tmp);
    auxenc_init(auxenc);
}

void auxenc_add_record(auxenc_t       *auxenc,
                       const char     *qname,
                       const uint16_t flag,
                       const char     *rname,
                       const uint8_t  mapq,
                       const char     *rnext,
                       const uint32_t pnext,
                       const int64_t  tlen,
                       const char     *opt)
{
    auxenc->in_sz += strlen(qname);
    auxenc->in_sz += sizeof(flag);
    auxenc->in_sz += strlen(rname);
    auxenc->in_sz += sizeof(mapq);
    auxenc->in_sz += strlen(rnext);
    auxenc->in_sz += sizeof(pnext);
    auxenc->in_sz += sizeof(tlen);
    auxenc->in_sz += strlen(opt);

    auxenc->rec_cnt++;

    char flag_cstr[101];
    char mapq_cstr[101];
    char pnext_cstr[101];
    char tlen_cstr[101];

    snprintf(flag_cstr, sizeof(flag_cstr), "%"PRIu16, flag);
    snprintf(mapq_cstr, sizeof(mapq_cstr), "%"PRIu8, mapq);
    snprintf(pnext_cstr, sizeof(pnext_cstr), "%"PRIu32, pnext);
    snprintf(tlen_cstr, sizeof(tlen_cstr), "%"PRId64, tlen);

    str_append_cstr(auxenc->tmp, qname);
    str_append_cstr(auxenc->tmp, "\t");
    str_append_cstr(auxenc->tmp, flag_cstr);
    str_append_cstr(auxenc->tmp, "\t");
    str_append_cstr(auxenc->tmp, rname);
    str_append_cstr(auxenc->tmp, "\t");
    str_append_cstr(auxenc->tmp, mapq_cstr);
    str_append_cstr(auxenc->tmp, "\t");
    str_append_cstr(auxenc->tmp, rnext);
    str_append_cstr(auxenc->tmp, "\t");
    str_append_cstr(auxenc->tmp, pnext_cstr);
    str_append_cstr(auxenc->tmp, "\t");
    str_append_cstr(auxenc->tmp, tlen_cstr);
    str_append_cstr(auxenc->tmp, "\t");
    str_append_cstr(auxenc->tmp, opt);
    str_append_cstr(auxenc->tmp, "\n");
}

size_t auxenc_write_block(auxenc_t *auxenc, FILE *fp)
{
    size_t ret = 0;

    // Compress block
    unsigned char *tmp = (unsigned char *)auxenc->tmp->s;
    unsigned int tmp_sz = (unsigned int)auxenc->tmp->len;
    unsigned int data_sz = 0;
    unsigned char *data = range_compress_o0(tmp, tmp_sz, &data_sz);

    // Compute CRC64
    uint64_t data_crc = crc64(data, data_sz);

    // Write compressed block
    unsigned char blk_id[8] = "aux----"; blk_id[7] = '\0';
    ret += fwrite_buf(fp, blk_id, sizeof(blk_id));
    ret += fwrite_uint64(fp, (uint64_t)auxenc->rec_cnt);
    ret += fwrite_uint64(fp, (uint64_t)data_sz);
    ret += fwrite_uint64(fp, (uint64_t)data_crc);
    ret += fwrite_buf(fp, data, data_sz);

    tsc_vlog("Compressed aux block: %zu bytes -> %zu bytes (%6.2f%%)\n",
             auxenc->in_sz,
             data_sz,
             (double)data_sz / (double)auxenc->in_sz * 100);

    auxenc_reset(auxenc); // Reset encoder for next block
    free(data); // Free memory used for encoded bitstream

    return ret;
}

// Decoder
// -----------------------------------------------------------------------------

static void auxdec_init(auxdec_t *auxdec)
{
    auxdec->out_sz = 0;
}

auxdec_t * auxdec_new(void)
{
    auxdec_t *auxdec = (auxdec_t *)tsc_malloc(sizeof(auxdec_t));
    auxdec_init(auxdec);
    return auxdec;
}

void auxdec_free(auxdec_t *auxdec)
{
    if (auxdec != NULL) {
        free(auxdec);
        auxdec = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

static void auxdec_reset(auxdec_t* auxdec)
{
    auxdec_init(auxdec);
}

static size_t auxdec_decode(auxdec_t      *auxdec,
                            unsigned char *tmp,
                            size_t         tmp_sz,
                            str_t         **qname,
                            uint16_t      *flag,
                            str_t         **rname,
                            uint8_t       *mapq,
                            str_t         **rnext,
                            uint32_t      *pnext,
                            int64_t       *tlen,
                            str_t         **opt)
{
    size_t ret = 0;
    size_t i = 0;
    size_t line = 0;
    unsigned char *cstr = tmp;
    unsigned int idx = 0;

    for (i = 0; i < tmp_sz; i++) {
        if (tmp[i] == '\n') {
            tmp[i] = '\0';
            str_append_cstr(opt[line], (const char *)cstr);
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
                str_append_cstr(qname[line], (const char *)cstr);
                ret += strlen((const char *)cstr);
                break;
            case 1:
                flag[line] = strtoll((const char *)cstr, NULL, 10);
                ret += sizeof(*flag);
                break;
            case 2:
                str_append_cstr(rname[line], (const char *)cstr);
                ret += strlen((const char *)cstr);
                break;
            case 3:
                mapq[line] = strtoll((const char *)cstr, NULL, 10);
                ret += sizeof(*mapq);
                break;
            case 4:
                str_append_cstr(rnext[line], (const char *)cstr);
                ret += strlen((const char *)cstr);
                break;
            case 5:
                pnext[line] = strtoll((const char *)cstr, NULL, 10);
                ret += sizeof(*pnext);
                break;
            case 6:
                tlen[line] = strtoll((const char *)cstr, NULL, 10);
                ret += sizeof(*tlen);
                break;
            case 7:
                // Fall through (all upcoming columns are 'opt')
            default:
                str_append_cstr(opt[line], (const char *)cstr);
                ret += strlen((const char *)cstr);
                str_append_char(opt[line], '\t');
                ret++;
                break;
            }
            cstr = &tmp[i+1];
        }
    }

    return ret;
}

size_t auxdec_decode_block(auxdec_t *auxdec,
                           FILE     *fp,
                           str_t    **qname,
                           uint16_t *flag,
                           str_t    **rname,
                           uint8_t  *mapq,
                           str_t    **rnext,
                           uint32_t *pnext,
                           int64_t  *tlen,
                           str_t    **opt)
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
        tsc_error("CRC64 check failed for aux block\n");

    // Decompress block
    unsigned int tmp_sz = 0;
    unsigned char *tmp = range_decompress_o0(data, data_sz, &tmp_sz);
    free(data);

    // Decode block
    auxdec->out_sz = auxdec_decode(auxdec,
                                   tmp,
                                   tmp_sz,
                                   qname,
                                   flag,
                                   rname,
                                   mapq,
                                   rnext,
                                   pnext,
                                   tlen,
                                   opt);
    free(tmp); // Free memory used for decoded bitstream

    tsc_vlog("Decompressed aux block: %zu bytes -> %zu bytes (%6.2f%%)\n",
             data_sz,
             auxdec->out_sz,
             (double)auxdec->out_sz / (double)data_sz * 100);

    auxdec_reset(auxdec);

    return ret;
}

