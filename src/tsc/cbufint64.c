// Copyright 2015 Leibniz University Hannover (LUH)

#include "cbufint64.h"

#include <string.h>

#include "log.h"
#include "mem.h"

static void cbufint64_init(cbufint64_t *cbufint64, const size_t sz) {
    cbufint64->sz = sz;
    cbufint64->nxt = 0;
    cbufint64->n = 0;
}

cbufint64_t *cbufint64_new(const size_t sz) {
    cbufint64_t *cbufint64 = (cbufint64_t *)malloc(sizeof(cbufint64_t));
    if (!cbufint64) abort();
    cbufint64->buf = (int64_t *)malloc(sizeof(int64_t) * sz);
    if (!cbufint64->buf) abort();
    memset(cbufint64->buf, 0x00, sz * sizeof(int64_t));
    cbufint64_init(cbufint64, sz);
    return cbufint64;
}

void cbufint64_free(cbufint64_t *cbufint64) {
    if (cbufint64 != NULL) {
        tsc_free(cbufint64->buf);
        tsc_free(cbufint64);
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

void cbufint64_clear(cbufint64_t *cbufint64) {
    memset(cbufint64->buf, 0x00, cbufint64->sz * sizeof(int64_t));
    cbufint64->nxt = 0;
    cbufint64->n = 0;
}

void cbufint64_push(cbufint64_t *cbufint64, int64_t x) {
    cbufint64->buf[cbufint64->nxt++] = x;
    if (cbufint64->nxt == cbufint64->sz) cbufint64->nxt = 0;
    if (cbufint64->n < cbufint64->sz) cbufint64->n++;
}

int64_t cbufint64_get(const cbufint64_t *cbufint64, size_t pos) {
    if (cbufint64->n == 0) {
        tsc_error("Tried to access empty cbufint64\n");
    }
    if (pos > (cbufint64->n - 1)) {
        tsc_error("Not enough elements in cbufint64\n");
    }

    return cbufint64->buf[pos];
}
