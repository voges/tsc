// Copyright 2015 Leibniz University Hannover (LUH)

#ifndef TSC_CBUFSTR_H_
#define TSC_CBUFSTR_H_

#include <stdlib.h>

#include "str.h"

typedef struct cbufstr_t_ {
    size_t sz;
    str_t **buf;
    size_t nxt;
    size_t n;
} cbufstr_t;

cbufstr_t *cbufstr_new(size_t sz);

void cbufstr_free(cbufstr_t *cbufstr);

void cbufstr_clear(cbufstr_t *cbufstr);

void cbufstr_push(cbufstr_t *cbufstr, const char *s);

str_t *cbufstr_get(const cbufstr_t *cbufstr, size_t pos);

#endif  // TSC_CBUFSTR_H_
