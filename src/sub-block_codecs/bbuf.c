/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#include "bbuf.h"
#include "../tsclib.h"
#include <string.h>

static void bbuf_init(bbuf_t* bb)
{
    bb->bytes = NULL;
    bb->sz = 0;
}

bbuf_t* bbuf_new(void)
{
    bbuf_t* bb = (bbuf_t*)tsc_malloc(sizeof(bbuf_t));
    bbuf_init(bb);
    return bb;
}

void bbuf_free(bbuf_t* bb)
{
    if (bb != NULL) {
        if (bb->bytes != NULL) {
            free(bb->bytes);
            bb->bytes = NULL;
        }
        free(bb);
        bb = NULL;
    } else { /* bb == NULL */
        tsc_error("Tried to free NULL pointer.\n");
    }
}

void bbuf_clear(bbuf_t* bb)
{
    if (bb->bytes != NULL) {
        free(bb->bytes);
        bb->bytes = NULL;
    }
    bb->sz = 0;
}

void bbuf_reserve(bbuf_t* bb, const size_t sz)
{
    bb->sz = sz;
    bb->bytes = (unsigned char*)tsc_realloc(bb->bytes, bb->sz);
}

void bbuf_extend(bbuf_t* bb, const size_t ex)
{
    bbuf_reserve(bb, bb->sz + ex);
}

void bbuf_trunc(bbuf_t* bb, const size_t tr)
{
    bb->sz -= tr;
    bbuf_reserve(bb, bb->sz);
}

void bbuf_append_bbuf(bbuf_t* bb, const bbuf_t* app)
{
    bbuf_extend(bb, app->sz);
    memcpy(bb->bytes + bb->sz, app->bytes, app->sz);
    bb->sz += app->sz;
}

void bbuf_append_byte(bbuf_t* bb, const char byte)
{
    bbuf_extend(bb, 1);
    bb->bytes[bb->sz++] = byte;
}

void bbuf_append_uint64(bbuf_t* bb, const uint64_t x)
{
    bbuf_append_byte(bb, (x >> 56) & 0xFF);
    bbuf_append_byte(bb, (x >> 48) & 0xFF);
    bbuf_append_byte(bb, (x >> 40) & 0xFF);
    bbuf_append_byte(bb, (x >> 32) & 0xFF);
    bbuf_append_byte(bb, (x >> 24) & 0xFF);
    bbuf_append_byte(bb, (x >> 16) & 0xFF);
    bbuf_append_byte(bb, (x >>  8) & 0xFF);
    bbuf_append_byte(bb, (x      ) & 0xFF);
}

void bbuf_append_buf(bbuf_t* bb, const unsigned char* app, const size_t n)
{
    bbuf_extend(bb, n);
    memcpy(bb->bytes + bb->sz, app, n);
    bb->sz += n;
}

