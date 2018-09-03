#include "support/bbuf.h"
#include <stdio.h>
#include <string.h>

static void bbuf_init(bbuf_t *bbuf)
{
    bbuf->bytes = NULL;
    bbuf->sz = 0;
}

bbuf_t * bbuf_new(void)
{
    bbuf_t *bbuf = (bbuf_t *)malloc(sizeof(bbuf_t));
    if (!bbuf) abort();
    bbuf_init(bbuf);
    return bbuf;
}

void bbuf_free(bbuf_t *bbuf)
{
    if (bbuf != NULL) {
        if (bbuf->bytes != NULL) {
            free(bbuf->bytes);
            bbuf->bytes = NULL;
        }
        free(bbuf);
        bbuf = NULL;
    } else {
        fprintf(stderr, "Error: Tried to free null pointer\n");
        exit(EXIT_FAILURE);
    }
}

void bbuf_clear(bbuf_t *bbuf)
{
    if (bbuf->bytes != NULL) {
        free(bbuf->bytes);
        bbuf->bytes = NULL;
    }
    bbuf->sz = 0;
}

void bbuf_reserve(bbuf_t *bbuf, const size_t sz)
{
    bbuf->sz = sz;
    bbuf->bytes = (unsigned char *)realloc(bbuf->bytes, bbuf->sz);
    if (!bbuf->bytes) abort();
}

void bbuf_extend(bbuf_t *bbuf, const size_t ex)
{
    bbuf_reserve(bbuf, bbuf->sz + ex);
}

void bbuf_trunc(bbuf_t *bbuf, const size_t tr)
{
    bbuf->sz -= tr;
    bbuf_reserve(bbuf, bbuf->sz);
}

void bbuf_append_bbuf(bbuf_t *bbuf, const bbuf_t *app)
{
    bbuf_extend(bbuf, app->sz);
    memcpy(bbuf->bytes + bbuf->sz, app->bytes, app->sz);
    bbuf->sz += app->sz;
}

void bbuf_append_byte(bbuf_t *bbuf, const unsigned char byte)
{
    bbuf_extend(bbuf, 1);
    bbuf->bytes[bbuf->sz++] = byte;
}

void bbuf_append_uint64(bbuf_t *bbuf, const uint64_t x)
{
    bbuf_append_byte(bbuf, (unsigned char)(x >> 56) & 0xFF);
    bbuf_append_byte(bbuf, (unsigned char)(x >> 48) & 0xFF);
    bbuf_append_byte(bbuf, (unsigned char)(x >> 40) & 0xFF);
    bbuf_append_byte(bbuf, (unsigned char)(x >> 32) & 0xFF);
    bbuf_append_byte(bbuf, (unsigned char)(x >> 24) & 0xFF);
    bbuf_append_byte(bbuf, (unsigned char)(x >> 16) & 0xFF);
    bbuf_append_byte(bbuf, (unsigned char)(x >>  8) & 0xFF);
    bbuf_append_byte(bbuf, (unsigned char)(x      ) & 0xFF);
}

void bbuf_append_buf(bbuf_t *bbuf, const unsigned char *buf, const size_t n)
{
    bbuf_extend(bbuf, n);
    memcpy(bbuf->bytes + bbuf->sz, buf, n);
    bbuf->sz += n;
}

