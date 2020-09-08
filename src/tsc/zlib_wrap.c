// Copyright 2015 Leibniz University Hannover (LUH)

#include "zlib_wrap.h"

#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

#include "log.h"

unsigned char *zlib_compress(unsigned char *in, size_t in_sz, size_t *out_sz) {
    Byte *out;
    compressBound(in_sz);
    *out_sz = compressBound(in_sz) + 1;
    out = (Byte *)calloc((uInt)*out_sz, 1);
    int err = compress(out, out_sz, (const Bytef *)in, (uLong)in_sz);
    if (err != Z_OK) {
        tsc_error("zlib failed to compress (error code: %d)\n", err);
    }
    return out;
}

unsigned char *zlib_decompress(unsigned char *in, size_t in_sz, size_t out_sz) {
    Bytef *out = (Bytef *)malloc(out_sz);
    if (!out) abort();
    int err = uncompress(out, (uLongf *)&out_sz, (const Bytef *)in, (uLong)in_sz);
    if (err != Z_OK) {
        tsc_error("zlib failed to uncompress (error code: %d)\n", err);
    }
    return out;
}
