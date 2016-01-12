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
// Aux block format:
//   unsigned char blk_id[8]: "aux----" + '\0'
//   uint64_t      rec_cnt  : No. of lines in block
//   uint64_t      data_sz  : Data size
//   uint64_t      data_crc : CRC64 of compressed data
//   unsigned char *data    : Data
//

#include "auxcodec.h"
#include "range.h"
#include "tsclib.h"
#include "common/crc64.h"
#include "tnt.h"
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
    auxenc_t *auxenc = (auxenc_t *)tnt_malloc(sizeof(auxenc_t));
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
                       const uint16_t flag,
                       const uint8_t  mapq,
                       const char     *opt)
{
    auxenc->in_sz += sizeof(flag);
    auxenc->in_sz += sizeof(mapq);
    auxenc->in_sz += strlen(opt);

    auxenc->rec_cnt++;

    char flag_cstr[101];
    char mapq_cstr[101];

    snprintf(flag_cstr, sizeof(flag_cstr), "%"PRIu16, flag);
    snprintf(mapq_cstr, sizeof(mapq_cstr), "%"PRIu8, mapq);

    str_append_cstr(auxenc->tmp, flag_cstr);
    str_append_cstr(auxenc->tmp, "\t");
    str_append_cstr(auxenc->tmp, mapq_cstr);
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
    ret += tnt_fwrite_buf(fp, blk_id, sizeof(blk_id));
    ret += tnt_fwrite_uint64(fp, (uint64_t)auxenc->rec_cnt);
    ret += tnt_fwrite_uint64(fp, (uint64_t)data_sz);
    ret += tnt_fwrite_uint64(fp, (uint64_t)data_crc);
    ret += tnt_fwrite_buf(fp, data, data_sz);

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
    auxdec_t *auxdec = (auxdec_t *)tnt_malloc(sizeof(auxdec_t));
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
                            uint16_t      *flag,
                            uint8_t       *mapq,
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
                flag[line] = strtoll((const char *)cstr, NULL, 10);
                ret += sizeof(*flag);
                break;
            case 1:
                mapq[line] = strtoll((const char *)cstr, NULL, 10);
                ret += sizeof(*mapq);
                break;
            case 2:
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
                           uint16_t *flag,
                           uint8_t  *mapq,
                           str_t    **opt)
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
        tsc_error("CRC64 check failed for aux block\n");

    // Decompress block
    unsigned int tmp_sz = 0;
    unsigned char *tmp = range_decompress_o0(data, data_sz, &tmp_sz);
    free(data);

    // Decode block
    auxdec->out_sz = auxdec_decode(auxdec, tmp, tmp_sz, flag, mapq, opt);
    free(tmp); // Free memory used for decoded bitstream

    auxdec_reset(auxdec);

    return ret;
}

