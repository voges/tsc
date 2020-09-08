// Copyright 2015 Leibniz University Hannover (LUH)

#include "cbufstr.h"

#include <stdio.h>

#include "log.h"
#include "mem.h"

static void cbufstr_init(cbufstr_t *cbufstr, const size_t sz) {
    cbufstr->sz = sz;
    cbufstr->nxt = 0;
    cbufstr->n = 0;
}

cbufstr_t *cbufstr_new(const size_t sz) {
    cbufstr_t *cbufstr = (cbufstr_t *)malloc(sizeof(cbufstr_t));
    if (!cbufstr) abort();
    cbufstr->buf = (str_t **)malloc(sizeof(str_t *) * sz);
    if (!cbufstr->buf) abort();
    for (size_t i = 0; i < sz; i++) cbufstr->buf[i] = str_new();
    cbufstr_init(cbufstr, sz);
    return cbufstr;
}

void cbufstr_free(cbufstr_t *cbufstr) {
    if (cbufstr != NULL) {
        for (size_t i = 0; i < cbufstr->sz; i++) str_free(cbufstr->buf[i]);
        tsc_free(cbufstr->buf);
        tsc_free(cbufstr);
    } else {
        tsc_error("Tried to free null pointer\n");
    }
}

void cbufstr_clear(cbufstr_t *cbufstr) {
    for (size_t i = 0; i < cbufstr->sz; i++) str_clear(cbufstr->buf[i]);
    cbufstr->nxt = 0;
    cbufstr->n = 0;
}

void cbufstr_push(cbufstr_t *cbufstr, const char *s) {
    str_copy_cstr(cbufstr->buf[cbufstr->nxt++], s);
    if (cbufstr->nxt == cbufstr->sz) cbufstr->nxt = 0;
    if (cbufstr->n < cbufstr->sz) cbufstr->n++;
}

str_t *cbufstr_get(const cbufstr_t *cbufstr, size_t pos) {
    if (cbufstr->n == 0) {
        tsc_error("Tried to access empty cbufstr\n");
    }
    if (pos > (cbufstr->n - 1)) {
        tsc_error("Not enough elements in cbufstr\n");
    }

    return cbufstr->buf[pos];
}
