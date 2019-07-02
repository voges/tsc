//
// Aux block format:
//   unsigned char id[8]          :"aux----" + '\0'
//   uint64_t      record_cnt     : No. of lines in block
//   uint64_t      uncompressed_sz: Size of uncompressed data
//   uint64_t      compressed_sz  : Compressed data size
//   uint64_t      compressed_crc : CRC64 of compressed data
//   unsigned char *compressed    : Compressed data
//

#include "auxcodec.h"
#include "zlib-wrap.h"
#include "fio.h"
#include "log.h"
#include "mem.h"
#include "crc64.h"
#include <inttypes.h>
#include <string.h>

static void auxcodec_init(auxcodec_t *auxcodec)
{
    auxcodec->record_cnt = 0;
    str_clear(auxcodec->uncompressed);
    if (auxcodec->compressed != NULL) free(auxcodec->compressed);
    auxcodec->compressed = NULL;
}

auxcodec_t * auxcodec_new(void)
{
    auxcodec_t *auxcodec = (auxcodec_t *)tsc_malloc(sizeof(auxcodec_t));
    auxcodec->uncompressed = str_new();
    auxcodec->compressed = NULL;
    auxcodec_init(auxcodec);
    return auxcodec;
}

void auxcodec_free(auxcodec_t *auxcodec)
{
    if (auxcodec != NULL) {
        str_free(auxcodec->uncompressed);
        if (auxcodec->compressed != NULL) {
            free(auxcodec->compressed);
            auxcodec->compressed = NULL;
        }
        free(auxcodec);
        // auxcodec = NULL;
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

// Encoder methods
// -----------------------------------------------------------------------------

void auxcodec_add_record(auxcodec_t     *auxcodec,
                         const uint16_t flag,
                         const uint8_t  mapq,
                         const char     *opt)
{
    auxcodec->record_cnt++;

    char flag_cstr[101];
    char mapq_cstr[101];

    snprintf(flag_cstr, sizeof(flag_cstr), "%"PRIu16, flag);
    snprintf(mapq_cstr, sizeof(mapq_cstr), "%"PRIu8, mapq);

    str_append_cstr(auxcodec->uncompressed, flag_cstr);
    str_append_cstr(auxcodec->uncompressed, "\t");
    str_append_cstr(auxcodec->uncompressed, mapq_cstr);
    str_append_cstr(auxcodec->uncompressed, "\t");
    str_append_cstr(auxcodec->uncompressed, opt);
    str_append_cstr(auxcodec->uncompressed, "\n");
}

size_t auxcodec_write_block(auxcodec_t *auxcodec, FILE *fp)
{
    size_t ret = 0;

    // Compress block
    unsigned char *uncompressed = (unsigned char *)auxcodec->uncompressed->s;
    size_t uncompressed_sz = auxcodec->uncompressed->len;
    size_t compressed_sz = 0;
    unsigned char *compressed = zlib_compress(uncompressed, uncompressed_sz, &compressed_sz);

    // Compute CRC64
    uint64_t compressed_crc = crc64(compressed, compressed_sz);

    // Write compressed block
    unsigned char id[8] = "aux----"; id[7] = '\0';
    ret += tsc_fwrite_buf(fp, id, sizeof(id));
    ret += tsc_fwrite_uint64(fp, (uint64_t)auxcodec->record_cnt);
    ret += tsc_fwrite_uint64(fp, (uint64_t)uncompressed_sz);
    ret += tsc_fwrite_uint64(fp, (uint64_t)compressed_sz);
    ret += tsc_fwrite_uint64(fp, (uint64_t)compressed_crc);
    ret += tsc_fwrite_buf(fp, compressed, compressed_sz);

    // Free memory allocated by zlib_compress
    free(compressed);

    auxcodec_init(auxcodec);

    return ret;
}

// Decoder methods
// -----------------------------------------------------------------------------

static size_t auxcodec_decode(unsigned char *tmp,
                              size_t        tmp_sz,
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
                flag[line] = (uint16_t)strtoul((const char *)cstr, NULL, 10);
                ret += sizeof(*flag);
                break;
            case 1:
                mapq[line] = (uint8_t)strtoul((const char *)cstr, NULL, 10);
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

size_t auxcodec_decode_block(auxcodec_t *auxcodec,
                            FILE        *fp,
                            uint16_t    *flag,
                            uint8_t     *mapq,
                            str_t       **opt)
{
    size_t ret = 0;

    unsigned char id[8];
    uint64_t      record_cnt;
    uint64_t      uncompressed_sz;
    uint64_t      compressed_sz;
    uint64_t      compressed_crc;
    unsigned char *compressed;

    // Read block
    ret += tsc_fread_buf(fp, id, sizeof(id));
    ret += tsc_fread_uint64(fp, &record_cnt);
    ret += tsc_fread_uint64(fp, &uncompressed_sz);
    ret += tsc_fread_uint64(fp, &compressed_sz);
    ret += tsc_fread_uint64(fp, &compressed_crc);
    compressed = (unsigned char *)tsc_malloc((size_t)compressed_sz);
    ret += tsc_fread_buf(fp, compressed, compressed_sz);

    // Check CRC64
    if (crc64(compressed, compressed_sz) != compressed_crc)
        tsc_error("CRC64 check failed for aux block\n");

    // Decompress block
    unsigned char *uncompressed = zlib_decompress(compressed, compressed_sz, uncompressed_sz);
    free(compressed);

    // Decode block
    auxcodec_decode(uncompressed, uncompressed_sz, flag, mapq, opt);
    free(uncompressed); // Free memory used for decoded bitstream

    auxcodec_init(auxcodec);

    return ret;
}
