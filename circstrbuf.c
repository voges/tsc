/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "circstrbuf.h"
#include "tsclib.h"

static void circstrbuf_init(circstrbuf_t* circstrbuf, unsigned int n)
{
    circstrbuf->n = n;
    circstrbuf->pos = 0;
}

circstrbuf_t* circstrbuf_new(unsigned int n)
{
    circstrbuf_t* circstrbuf = (circstrbuf_t*)tsc_malloc_or_die(sizeof(circstrbuf_t));
    circstrbuf->buf = (str_t**)tsc_malloc_or_die(sizeof(str_t*) * n);
    unsigned int i = 0;
    for (i = 0; i < n; i++) {
        circstrbuf->buf[i] = str_new();
    }
    circstrbuf_init(circstrbuf, n);
    return circstrbuf;
}

void circstrbuf_free(circstrbuf_t* circstrbuf)
{
    if (circstrbuf != NULL) {
        unsigned int i = 0;
        for (i = 0; i < circstrbuf->n; i++) {
            str_free(circstrbuf->buf[i]);
        }
        free((void*)circstrbuf->buf);
        free((void*)circstrbuf);
        circstrbuf = NULL;
    } else { /* circstrbuf == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

void circstrbuf_add(circstrbuf_t* circstrbuf, const char* s)
{
    str_clear(circstrbuf->buf[circstrbuf->pos]);
    str_append_cstr(circstrbuf->buf[circstrbuf->pos], s);
    circstrbuf->pos++;
    if (circstrbuf->pos == circstrbuf->n)
        circstrbuf->pos = 0;
}

str_t* circstrbuf_elem(circstrbuf_t* circstrbuf, unsigned int pos)
{
    if (pos >= circstrbuf->n) {
        tsc_error("circstrbuf: Tried to access element out of range.");
        return NULL;
    }
    return circstrbuf->buf[pos];
}

