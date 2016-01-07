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
// Id block format:
//   unsigned char  blk_id[8]: "id-----" + '\0'
//   uint64_t       rec_cnt  : No. of lines in block
//   uint64_t       data_sz  : Data size
//   uint64_t       data_crc : CRC64 of comptmpsed data
//   unsigned char  *data    : Data
//

#include "idcodec.h"
#include "../range.h"
#include "../tsclib.h"
#include "../tvclib/crc64.h"
#include "../tvclib/frw.h"
#include <string.h>

// Encoder
// -----------------------------------------------------------------------------

static void idenc_init(idenc_t *idenc)
{
    idenc->in_sz = 0;
    idenc->rec_cnt = 0;
}

idenc_t * idenc_new(void)
{
    idenc_t *idenc = (idenc_t *)tsc_malloc(sizeof(idenc_t));
    idenc->tmp = str_new();
    idenc_init(idenc);
    return idenc;
}

void idenc_free(idenc_t* idenc)
{
    if (idenc != NULL) {
        str_free(idenc->tmp);
        free(idenc);
        idenc = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

static void idenc_reset(idenc_t *idenc)
{
    str_clear(idenc->tmp);
    idenc_init(idenc);
}

void idenc_add_record(idenc_t *idenc, const char *qname)
{
    idenc->in_sz += strlen(qname);

    idenc->rec_cnt++;

    str_append_cstr(idenc->tmp, qname);
    str_append_cstr(idenc->tmp, "\n");
}

size_t idenc_write_block(idenc_t *idenc, FILE * fp)
{
    size_t ret = 0;

    // Compress block
    unsigned char *tmp = (unsigned char *)idenc->tmp->s;
    unsigned int tmp_sz = (unsigned int)idenc->tmp->len;
    unsigned int data_sz = 0;
    unsigned char *data = range_compress_o1(tmp, tmp_sz, &data_sz);

    // Compute CRC64
    uint64_t data_crc = crc64(data, data_sz);

    // Write compressed block
    unsigned char blk_id[8] = "id-----"; blk_id[7] = '\0';
    ret += fwrite_buf(fp, blk_id, sizeof(blk_id));
    ret += fwrite_uint64(fp, (uint64_t)idenc->rec_cnt);
    ret += fwrite_uint64(fp, (uint64_t)data_sz);
    ret += fwrite_uint64(fp, (uint64_t)data_crc);
    ret += fwrite_buf(fp, data, data_sz);

    tsc_log(TSC_LOG_VERBOSE,
            "Compressed id block: %zu bytes -> %zu bytes (%6.2f%%)\n",
            idenc->in_sz,
            data_sz,
            (double)data_sz / (double)idenc->in_sz * 100);

    idenc_reset(idenc); // Reset encoder for next block
    free(data); // Free memory used for encoded bitstream

    return ret;
}

// Decoder
// -----------------------------------------------------------------------------

static void iddec_init(iddec_t *iddec)
{
    iddec->out_sz = 0;
}

iddec_t * iddec_new(void)
{
    iddec_t *iddec = (iddec_t *)tsc_malloc(sizeof(iddec_t));
    iddec_init(iddec);
    return iddec;
}

void iddec_free(iddec_t *iddec)
{
    if (iddec != NULL) {
        free(iddec);
        iddec = NULL;
    } else {
        tsc_error("Tried to free null\n");
    }
}

static void iddec_reset(iddec_t *iddec)
{
    iddec_init(iddec);
}

static size_t iddec_decode(iddec_t       *iddec,
                           unsigned char *tmp,
                           size_t        tmp_sz,
                           str_t**       qname)
{
    size_t ret = 0;
    size_t i = 0;
    size_t rec = 0;

    for (i = 0; i < tmp_sz; i++) {
        if (tmp[i] != '\n') {
            str_append_char(qname[rec], (const char)tmp[i]);
            ret++;
        } else {
            rec++;
        }
    }

    return ret;
}

size_t iddec_decode_block(iddec_t *iddec, FILE *fp, str_t **qname)
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
        tsc_error("CRC64 check failed for id block\n");

    // Decompress block
    unsigned int tmp_sz = 0;
    unsigned char *tmp = range_decompress_o1(data, data_sz, &tmp_sz);
    free(data);

    // Decode block
    iddec->out_sz = iddec_decode(iddec, tmp, tmp_sz, qname);
    free(tmp); // Free memory used for decoded bitstream

    tsc_log(TSC_LOG_VERBOSE,
            "Decompressed id block: %zu bytes -> %zu bytes (%6.2f%%)\n",
            data_sz,
            iddec->out_sz,
            (double)iddec->out_sz / (double)data_sz * 100);

    iddec_reset(iddec);

    return ret;
}

