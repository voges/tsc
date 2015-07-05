/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "cbufstr.h"
#include "../tsclib.h"

static void cbufstr_init(cbufstr_t* cbufstr, const size_t sz)
{
    cbufstr->sz = sz;
    cbufstr->nxt = 0;
    cbufstr->n = 0;
}

cbufstr_t* cbufstr_new(const size_t sz)
{
    cbufstr_t* cbufstr = (cbufstr_t*)tsc_malloc(sizeof(cbufstr_t));
    cbufstr->buf = (str_t**)tsc_malloc(sizeof(str_t*) * sz);
    unsigned int i = 0;
    for (i = 0; i < sz; i++) cbufstr->buf[i] = str_new();
    cbufstr_init(cbufstr, sz);
    return cbufstr;
}

void cbufstr_free(cbufstr_t* cbufstr)
{
    if (cbufstr != NULL) {
        unsigned int i = 0;
        for (i = 0; i < cbufstr->sz; i++) str_free(cbufstr->buf[i]);
        free((void*)cbufstr->buf);
        free((void*)cbufstr);
        cbufstr = NULL;
    } else { /* cbufstr == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

void cbufstr_clear(cbufstr_t* cbufstr)
{
    unsigned int i = 0;
    for (i = 0; i < cbufstr->sz; i++) str_clear(cbufstr->buf[i]);
    cbufstr->nxt = 0;
    cbufstr->n = 0;
}

void cbufstr_push(cbufstr_t* cbufstr, const char* s)
{
    str_copy_cstr(cbufstr->buf[cbufstr->nxt++], s);
    if (cbufstr->nxt == cbufstr->sz) cbufstr->nxt = 0;
    if (cbufstr->n < cbufstr->sz) cbufstr->n++;
}

str_t* cbufstr_top(cbufstr_t* cbufstr)
{
    if (cbufstr->n == 0) tsc_error("Tried to access empty buffer!\n");
    size_t nxt = cbufstr->nxt;
    size_t last = 0;
    if (nxt == 0) last = cbufstr->sz - 1;
    else last = cbufstr->nxt - 1;
    return cbufstr->buf[last];
}

str_t* cbufstr_get(const cbufstr_t* cbufstr, size_t pos)
{
    if (cbufstr->n == 0) tsc_error("Tried to access empty buffer!\n");
    if (pos > (cbufstr->n - 1)) tsc_error("Not enough elements in buffer!\n");
    return cbufstr->buf[pos];
}

