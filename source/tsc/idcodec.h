#ifndef TSC_IDCODEC_H
#define TSC_IDCODEC_H

#include "str.h"
#include <stdio.h>

typedef struct idcodec_t_ {
    size_t        record_cnt; // No. of records processed in the current block
    str_t         *uncompressed;
    unsigned char *compressed;
} idcodec_t;

idcodec_t * idcodec_new(void);
void idcodec_free(idcodec_t *idcodec);

// Encoder methods
// -----------------------------------------------------------------------------

void idcodec_add_record(idcodec_t *idcodec, const char *qname);
size_t idcodec_write_block(idcodec_t *idcodec, FILE *fp);

// Decoder methods
// -----------------------------------------------------------------------------

size_t idcodec_decode_block(idcodec_t *idcodec, FILE *fp, str_t **qname);

#endif // TSC_IDCODEC_H
