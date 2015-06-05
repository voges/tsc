/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "cbufstr.h"
#include "tsclib.h"

static void cbufstr_init(cbufstr_t* cbufstr, const size_t sz)
{
    cbufstr->sz = sz;
    cbufstr->pos = 0;
    cbufstr->n = 0;
}

cbufstr_t* cbufstr_new(const size_t sz)
{
    cbufstr_t* cbufstr = (cbufstr_t*)tsc_malloc_or_die(sizeof(cbufstr_t));
    cbufstr->buf = (str_t**)tsc_malloc_or_die(sizeof(str_t*) * sz);
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
    cbufstr->pos = 0;
    cbufstr->n = 0;
}

void cbufstr_push(cbufstr_t* cbufstr, const char* s)
{
    str_copy_cstr(cbufstr->buf[cbufstr->pos++], s);
    if (cbufstr->pos == cbufstr->sz) cbufstr->pos = 0;
    if (cbufstr->n <= cbufstr->sz) cbufstr->n++;
}

str_t* cbufstr_get(const cbufstr_t* cbufstr, size_t pos)
{
    if (pos > cbufstr->n) {
        tsc_error("Tried to access element out of range in circular buffer!\n");
    }
    return cbufstr->buf[pos];
}

