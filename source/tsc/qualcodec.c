//
// Qual block format:
//   unsigned char id[8]          : "qual---" + '\0'
//   uint64_t      record_cnt     : No. of lines in block
//   uint64_t      uncompressed_sz: Size of uncompressed data
//   uint64_t      compressed_sz  : Compressed data size
//   uint64_t      compressed_crc : CRC64 of compressed data
//   unsigned char *compressed    : Compressed data
//

#include "qualcodec.h"
#include <string.h>
#include "crc64.h"
#include "fio.h"
#include "log.h"
#include "mem.h"
#include "zlib-wrap.h"

static void qualcodec_init(qualcodec_t *qualcodec) {
    qualcodec->record_cnt = 0;
    str_clear(qualcodec->uncompressed);
    if (qualcodec->compressed != NULL) {
        free(qualcodec->compressed);
        qualcodec->compressed = NULL;
    }
}

qualcodec_t *qualcodec_new(void) {
    qualcodec_t *qualcodec = (qualcodec_t *)tsc_malloc(sizeof(qualcodec_t));
    qualcodec->uncompressed = str_new();
    qualcodec->compressed = NULL;
    qualcodec_init(qualcodec);
    return qualcodec;
}

void qualcodec_free(qualcodec_t *qualcodec) {
    if (qualcodec != NULL) {
        str_free(qualcodec->uncompressed);
        if (qualcodec->compressed != NULL) {
            free(qualcodec->compressed);
            qualcodec->compressed = NULL;
        }
        free(qualcodec);
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

// Encoder methods
// -----------------------------------------------------------------------------

void qualcodec_add_record(qualcodec_t *qualcodec, const char *qual) {
    qualcodec->record_cnt++;

    str_append_cstr(qualcodec->uncompressed, qual);
    str_append_cstr(qualcodec->uncompressed, "\n");
}

size_t qualcodec_write_block(qualcodec_t *qualcodec, FILE *fp) {
    size_t ret = 0;

    // Compress block
    unsigned char *uncompressed = (unsigned char *)qualcodec->uncompressed->s;
    size_t uncompressed_sz = qualcodec->uncompressed->len;
    size_t compressed_sz = 0;
    unsigned char *compressed =
        zlib_compress(uncompressed, uncompressed_sz, &compressed_sz);

    // Compute CRC64
    uint64_t compressed_crc = crc64(compressed, compressed_sz);

    // Write compressed block
    unsigned char id[8] = "qual---";
    id[7] = '\0';
    ret += tsc_fwrite_buf(fp, id, sizeof(id));
    ret += tsc_fwrite_uint64(fp, (uint64_t)qualcodec->record_cnt);
    ret += tsc_fwrite_uint64(fp, (uint64_t)uncompressed_sz);
    ret += tsc_fwrite_uint64(fp, (uint64_t)compressed_sz);
    ret += tsc_fwrite_uint64(fp, (uint64_t)compressed_crc);
    ret += tsc_fwrite_buf(fp, compressed, compressed_sz);

    // Free memory allocated by zlib_compress
    free(compressed);

    qualcodec_init(qualcodec);

    return ret;
}

// Decoder methods
// -----------------------------------------------------------------------------

static size_t qualcodec_decode(unsigned char *tmp, size_t tmp_sz,
                               str_t **qual) {
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

size_t qualcodec_decode_block(qualcodec_t *qualcodec, FILE *fp, str_t **qual) {
    size_t ret = 0;

    unsigned char id[8];
    uint64_t record_cnt;
    uint64_t uncompressed_sz;
    uint64_t compressed_sz;
    uint64_t compressed_crc;
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
        tsc_error("CRC64 check failed for qual block\n");

    // Decompress block
    unsigned char *uncompressed =
        zlib_decompress(compressed, compressed_sz, uncompressed_sz);
    free(compressed);

    // Decode block
    qualcodec_decode(uncompressed, uncompressed_sz, qual);
    free(uncompressed);  // Free memory used for decoded bitstream

    qualcodec_init(qualcodec);

    return ret;
}
