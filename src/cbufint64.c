/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "cbufint64.h"
#include "tsclib.h"
#include <string.h>

static void cbufint64_init(cbufint64_t* cbufint64, const size_t sz)
{
    cbufint64->sz = sz;
    cbufint64->pos = 0;
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

void cbufint64_push(cbufint64_t* cbufint64, int64_t x)
{
    cbufint64->buf[cbufint64->pos++] = x;
    if (cbufint64->pos == cbufint64->sz)
        cbufint64->pos = 0;
}

int64_t cbufint64_get(const cbufint64_t* cbufint64, size_t pos)
{
    if (pos >= cbufint64->sz) {
        tsc_error("Tried to access element out of range in circular buffer!\n");
    }
    return cbufint64->buf[pos];
}

