/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#ifndef TSC_BBUF_H
#define TSC_BBUF_H

/* Byte buffer */

#include <stdint.h>
#include <stdlib.h>

typedef struct bbuf_t_ {
    unsigned char*  bytes; /* byte buffer             */
    size_t          sz;    /* bytes allocated for buf */
} bbuf_t;

bbuf_t* bbuf_new(void);
void bbuf_free(bbuf_t* bbuf);
void bbuf_clear(bbuf_t* bbuf);
void bbuf_reserve(bbuf_t* bbuf, const size_t sz);
void bbuf_extend(bbuf_t* bbuf, const size_t ex);
void bbuf_trunc(bbuf_t* bbuf, const size_t tr);
void bbuf_append_bbuf(bbuf_t* bbuf, const bbuf_t* app);
void bbuf_append_byte(bbuf_t* bbuf, char c);
void bbuf_append_uint64(bbuf_t* bbuf, const uint64_t x);
void bbuf_copy_cbbuf(bbuf_t* dest, const char* src);

#endif /* TSC_BBUF_H */

