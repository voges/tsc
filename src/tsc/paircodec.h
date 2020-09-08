// Copyright 2015 Leibniz University Hannover (LUH)

#ifndef TSC_PAIRCODEC_H_
#define TSC_PAIRCODEC_H_

#include <stdint.h>
#include <stdio.h>

#include "str.h"

typedef struct paircodec_t_ {
    size_t record_cnt;
    str_t *uncompressed;
    unsigned char *compressed;
} paircodec_t;

paircodec_t *paircodec_new();

void paircodec_free(paircodec_t *paircodec);

// Encoder methods
// -----------------------------------------------------------------------------

void paircodec_add_record(paircodec_t *paircodec, const char *rnext, uint32_t pnext, int64_t tlen);

size_t paircodec_write_block(paircodec_t *paircodec, FILE *fp);

// Decoder methods
// -----------------------------------------------------------------------------

size_t paircodec_decode_block(paircodec_t *paircodec, FILE *fp, str_t **rnext, uint32_t *pnext, int64_t *tlen);

#endif  // TSC_PAIRCODEC_H_
