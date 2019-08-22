//
// Id block format:
//   unsigned char id[8]          : "id-----" + '\0'
//   uint64_t      record_cnt     : No. of lines in block
//   uint64_t      uncompressed_sz: Size of uncompressed data
//   uint64_t      compressed_sz  : Compressed data size
//   uint64_t      compressed_crc : CRC64 of compressed data
//   unsigned char *compressed    : Compressed data
//

#include "idcodec.h"
#include <string.h>
#include "crc64.h"
#include "fio.h"
#include "log.h"
#include "mem.h"
#include "zlib-wrap.h"

static void idcodec_init(idcodec_t *idcodec) {
    idcodec->record_cnt = 0;
    str_clear(idcodec->uncompressed);
    if (idcodec->compressed != NULL) {
        free(idcodec->compressed);
        idcodec->compressed = NULL;
    }
}

idcodec_t *idcodec_new(void) {
    idcodec_t *idcodec = (idcodec_t *)tsc_malloc(sizeof(idcodec_t));
    idcodec->uncompressed = str_new();
    idcodec->compressed = NULL;
    idcodec_init(idcodec);
    return idcodec;
}

void idcodec_free(idcodec_t *idcodec) {
    if (idcodec != NULL) {
        str_free(idcodec->uncompressed);
        if (idcodec->compressed != NULL) {
            free(idcodec->compressed);
            idcodec->compressed = NULL;
        }
        free(idcodec);
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

// Encoder methods
// -----------------------------------------------------------------------------

void idcodec_add_record(idcodec_t *idcodec, const char *qname) {
    idcodec->record_cnt++;

    str_append_cstr(idcodec->uncompressed, qname);
    str_append_cstr(idcodec->uncompressed, "\n");
}

size_t idcodec_write_block(idcodec_t *idcodec, FILE *fp) {
    size_t ret = 0;

    // Compress block
    unsigned char *uncompressed = (unsigned char *)idcodec->uncompressed->s;
    size_t uncompressed_sz = idcodec->uncompressed->len;
    size_t compressed_sz = 0;
    unsigned char *compressed =
        zlib_compress(uncompressed, uncompressed_sz, &compressed_sz);

    // Compute CRC64
    uint64_t compressed_crc = crc64(compressed, compressed_sz);

    // Write compressed block
    unsigned char id[8] = "id-----";
    id[7] = '\0';
    ret += tsc_fwrite_buf(fp, id, sizeof(id));
    ret += tsc_fwrite_uint64(fp, (uint64_t)idcodec->record_cnt);
    ret += tsc_fwrite_uint64(fp, (uint64_t)uncompressed_sz);
    ret += tsc_fwrite_uint64(fp, (uint64_t)compressed_sz);
    ret += tsc_fwrite_uint64(fp, (uint64_t)compressed_crc);
    ret += tsc_fwrite_buf(fp, compressed, compressed_sz);

    // Free memory allocated by zlib_compress
    free(compressed);

    idcodec_init(idcodec);

    return ret;
}

// Decoder methods
// -----------------------------------------------------------------------------

static size_t idcodec_decode(unsigned char *tmp, size_t tmp_sz, str_t **qname) {
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

size_t idcodec_decode_block(idcodec_t *idcodec, FILE *fp, str_t **qname) {
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
        tsc_error("CRC64 check failed for id block\n");

    // Decompress block
    unsigned char *uncompressed =
        zlib_decompress(compressed, compressed_sz, uncompressed_sz);
    free(compressed);

    // Decode block
    idcodec_decode(uncompressed, uncompressed_sz, qname);
    free(uncompressed);  // Free memory used for decoded bitstream

    idcodec_init(idcodec);

    return ret;
}
