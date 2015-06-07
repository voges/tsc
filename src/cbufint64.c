/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "cbufint64.h"
#include "tsclib.h"
#include <string.h>

static void cbufint64_init(cbufint64_t* cbufint64, const size_t sz)
{
    cbufint64->sz = sz;
    cbufint64->nxt = 0;
    cbufint64->n = 0;
}

cbufint64_t* cbufint64_new(const size_t sz)
{
    cbufint64_t* cbufint64 = (cbufint64_t*)tsc_malloc_or_die(sizeof(cbufint64_t));
    cbufint64->buf = (int64_t*)tsc_malloc_or_die(sizeof(int64_t) * sz);
    memset(cbufint64->buf, 0x00, sz * sizeof(int64_t));
    cbufint64_init(cbufint64, sz);
    return cbufint64;
}

void cbufint64_free(cbufint64_t* cbufint64)
{
    if (cbufint64 != NULL) {
        free((void*)cbufint64->buf);
        free((void*)cbufint64);
        cbufint64 = NULL;
    } else { /* cbufint64 == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

void cbufint64_clear(cbufint64_t* cbufint64)
{
    memset(cbufint64->buf, 0x00, cbufint64->sz * sizeof(int64_t));
    cbufint64->nxt = 0;
    cbufint64->n = 0;
}

void cbufint64_push(cbufint64_t* cbufint64, int64_t x)
{
    cbufint64->buf[cbufint64->nxt++] = x;
    if (cbufint64->nxt == cbufint64->sz) cbufint64->nxt = 0;
    if (cbufint64->n < cbufint64->sz) cbufint64->n++;
}

int64_t cbufint64_top(cbufint64_t* cbufint64)
{
    if (cbufint64->n == 0) tsc_error("Tried to access empty buffer!\n");
    
    size_t nxt = cbufint64->nxt;
    size_t last = 0;
    if (nxt == 0) last = cbufint64->sz - 1;
    else last = cbufint64->nxt - 1;
    return cbufint64->buf[last];
}

int64_t cbufint64_get(const cbufint64_t* cbufint64, size_t pos)
{
    if (cbufint64->n == 0) tsc_error("Tried to access empty buffer!\n");
    if (pos > cbufint64->n - 1) tsc_error("Not enough elements in buffer!\n");
    return cbufint64->buf[pos];
}

