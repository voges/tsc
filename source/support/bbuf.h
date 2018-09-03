#ifndef TSC_BBUF_H
#define TSC_BBUF_H

#include <stdint.h>
#include <stdlib.h>

typedef struct bbuf_t_ {
    unsigned char *bytes;
    size_t        sz;
} bbuf_t;

bbuf_t * bbuf_new(void);
void bbuf_free(bbuf_t *bbuf);
void bbuf_clear(bbuf_t *bbuf);
void bbuf_reserve(bbuf_t *bbuf, const size_t sz);
void bbuf_extend(bbuf_t *bbuf, const size_t ex);
void bbuf_trunc(bbuf_t *bbuf, const size_t tr);
void bbuf_append_bbuf(bbuf_t *bbuf, const bbuf_t *app);
void bbuf_append_byte(bbuf_t *bbuf, const unsigned char byte);
void bbuf_append_uint64(bbuf_t *bbuf, const uint64_t x);
void bbuf_append_buf(bbuf_t *bbuf, const unsigned char *buf, const size_t n);

#endif // TSC_BBUF_H

