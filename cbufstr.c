/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "cbufstr.h"
#include "tsclib.h"

static void cbufstr_init(cbufstr_t* cbufstr, const unsigned int sz)
{
    cbufstr->sz = sz;
    cbufstr->pos = 0;
}

cbufstr_t* cbufstr_new(const unsigned int sz)
{
    cbufstr_t* cbufstr = (cbufstr_t*)tsc_malloc_or_die(sizeof(cbufstr_t));
    cbufstr->buf = (str_t**)tsc_malloc_or_die(sizeof(str_t*) * sz);
    unsigned int i = 0;
    for (i = 0; i < sz; i++) {
        cbufstr->buf[i] = str_new();
    }
    cbufstr_init(cbufstr, sz);
    return cbufstr;
}

void cbufstr_free(cbufstr_t* cbufstr)
{
    if (cbufstr != NULL) {
        unsigned int i = 0;
        for (i = 0; i < cbufstr->sz; i++) {
            str_free(cbufstr->buf[i]);
        }
        free((void*)cbufstr->buf);
        free((void*)cbufstr);
        cbufstr = NULL;
    } else { /* cbufstr == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.\n");
    }
}

void cbufstr_add(cbufstr_t* cbufstr, const char* s)
{
    str_copy_cstr(cbufstr->buf[cbufstr->pos++], s);
    if (cbufstr->pos == cbufstr->sz)
        cbufstr->pos = 0;
}

str_t* cbufstr_elem(const cbufstr_t* cbufstr, unsigned int pos)
{
    if (pos >= cbufstr->sz) {
        tsc_error("cbufstr: Tried to access element out of range.\n");
        return NULL;
    }
    return cbufstr->buf[pos];
}

