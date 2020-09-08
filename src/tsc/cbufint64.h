// Copyright 2015 Leibniz University Hannover (LUH)

#ifndef TSC_CBUFINT64_H_
#define TSC_CBUFINT64_H_

#include <stdint.h>
#include <stdlib.h>

typedef struct cbufint64_t_ {
    size_t sz;
    int64_t *buf;
    size_t nxt;
    size_t n;
} cbufint64_t;

cbufint64_t *cbufint64_new(size_t sz);

void cbufint64_free(cbufint64_t *cbufint64);

void cbufint64_clear(cbufint64_t *cbufint64);

void cbufint64_push(cbufint64_t *cbufint64, int64_t x);

int64_t cbufint64_get(const cbufint64_t *cbufint64, size_t pos);

#endif  // TSC_CBUFINT64_H_
