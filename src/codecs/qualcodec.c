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
// Qual block format:
//   unsigned char  blk_id[8]: "qual---" + '\0'
//   uint64_t       rec_cnt  : No. of lines in block
//   uint64_t       data_sz  : Data size
//   uint64_t       data_crc : CRC64 of comptmpsed data
//   unsigned char  *data    : Data
//

#include "qualcodec.h"
#include "../range.h"
#include "../tsclib.h"
#include "../tvclib/crc64.h"
#include "../tvclib/frw.h"
#include <string.h>

// Encoder
// -----------------------------------------------------------------------------

static void qualenc_init(qualenc_t *qualenc)
{
    qualenc->in_sz = 0;
    qualenc->rec_cnt = 0;
}

qualenc_t * qualenc_new(void)
{
    qualenc_t *qualenc = (qualenc_t *)tsc_malloc(sizeof(qualenc_t));
    qualenc->tmp = str_new();
    qualenc_init(qualenc);
    return qualenc;
}

void qualenc_free(qualenc_t* qualenc)
{
    if (qualenc != NULL) {
        str_free(qualenc->tmp);
        free(qualenc);
        qualenc = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

static void qualenc_reset(qualenc_t *qualenc)
{
    str_clear(qualenc->tmp);
    qualenc_init(qualenc);
}

void qualenc_add_record(qualenc_t *qualenc, const char *qual)
{
    qualenc->in_sz += strlen(qual);

    qualenc->rec_cnt++;

    str_append_cstr(qualenc->tmp, qual);
    str_append_cstr(qualenc->tmp, "\n");
}

size_t qualenc_write_block(qualenc_t *qualenc, FILE * fp)
{
    size_t ret = 0;

    // Compress block
    unsigned char *tmp = (unsigned char *)qualenc->tmp->s;
    unsigned int tmp_sz = (unsigned int)qualenc->tmp->len;
    unsigned int data_sz = 0;
    unsigned char *data = range_compress_o1(tmp, tmp_sz, &data_sz);

    // Compute CRC64
    uint64_t data_crc = crc64(data, data_sz);

    // Write compressed block
    unsigned char blk_id[8] = "qual---"; blk_id[7] = '\0';
    ret += fwrite_buf(fp, blk_id, sizeof(blk_id));
    ret += fwrite_uint64(fp, (uint64_t)qualenc->rec_cnt);
    ret += fwrite_uint64(fp, (uint64_t)data_sz);
    ret += fwrite_uint64(fp, (uint64_t)data_crc);
    ret += fwrite_buf(fp, data, data_sz);

    tsc_log(TSC_LOG_VERBOSE,
            "Compressed qual block: %zu bytes -> %zu bytes (%6.2f%%)\n",
            qualenc->in_sz,
            data_sz,
            (double)data_sz / (double)qualenc->in_sz * 100);

    qualenc_reset(qualenc); // Reset encoder for next block
    free(data); // Free memory used for encoded bitstream

    return ret;
}

// Decoder
// -----------------------------------------------------------------------------

static void qualdec_init(qualdec_t *qualdec)
{
    qualdec->out_sz = 0;
}

qualdec_t * qualdec_new(void)
{
    qualdec_t *qualdec = (qualdec_t *)tsc_malloc(sizeof(qualdec_t));
    qualdec_init(qualdec);
    return qualdec;
}

void qualdec_free(qualdec_t *qualdec)
{
    if (qualdec != NULL) {
        free(qualdec);
        qualdec = NULL;
    } else {
        tsc_error("Tried to free null\n");
    }
}

static void qualdec_reset(qualdec_t *qualdec)
{
    qualdec_init(qualdec);
}

static size_t qualdec_decode(qualdec_t     *qualdec,
                             unsigned char *tmp,
                             size_t        tmp_sz,
                             str_t**       qual)
{
    size_t ret = 0;
    size_t i = 0;
    size_t rec = 0;

    for (i = 0; i < tmp_sz; i++) {
        if (tmp[i] != '\n') {
            str_append_char(qual[rec], (const char)tmp[i]);
            ret++;
        } else {
            rec++;
        }
    }

    return ret;
}

size_t qualdec_decode_block(qualdec_t *qualdec, FILE *fp, str_t **qual)
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
        tsc_error("CRC64 check failed for qual block\n");

    // Decompress block
    unsigned int tmp_sz = 0;
    unsigned char *tmp = range_decompress_o1(data, data_sz, &tmp_sz);
    free(data);

    // Decode block
    qualdec->out_sz = qualdec_decode(qualdec, tmp, tmp_sz, qual);
    free(tmp); // Free memory used for decoded bitstream

    tsc_log(TSC_LOG_VERBOSE,
            "Decompressed qual block: %zu bytes -> %zu bytes (%6.2f%%)\n",
            data_sz,
            qualdec->out_sz,
            (double)qualdec->out_sz / (double)data_sz * 100);

    qualdec_reset(qualdec);

    return ret;
}

