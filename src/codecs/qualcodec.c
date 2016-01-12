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
// Qual block format:
//   unsigned char blk_id[8]: "qual---" + '\0'
//   uint64_t      rec_cnt  : No. of lines in block
//   uint64_t      data_sz  : Data size
//   uint64_t      data_crc : CRC64 of comptmpsed data
//   unsigned char *data    : Data
//

#include "qualcodec.h"
#include "range.h"
#include "tsclib.h"
#include "common/crc64.h"
#include "tnt.h"
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
    qualenc_t *qualenc = (qualenc_t *)tnt_malloc(sizeof(qualenc_t));
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
    ret += tnt_fwrite_buf(fp, blk_id, sizeof(blk_id));
    ret += tnt_fwrite_uint64(fp, (uint64_t)qualenc->rec_cnt);
    ret += tnt_fwrite_uint64(fp, (uint64_t)data_sz);
    ret += tnt_fwrite_uint64(fp, (uint64_t)data_crc);
    ret += tnt_fwrite_buf(fp, data, data_sz);

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
    qualdec_t *qualdec = (qualdec_t *)tnt_malloc(sizeof(qualdec_t));
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
    tnt_fread_buf(fp, blk_id, sizeof(blk_id));
    tnt_fread_uint64(fp, &rec_cnt);
    tnt_fread_uint64(fp, &data_sz);
    tnt_fread_uint64(fp, &data_crc);
    data = (unsigned char *)tnt_malloc((size_t)data_sz);
    tnt_fread_buf(fp, data, data_sz);

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

    qualdec_reset(qualdec);

    return ret;
}

