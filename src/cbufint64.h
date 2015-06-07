/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#ifndef TSC_CBUFINT64_H
#define TSC_CBUFINT64_H

#include <stdint.h>
#include <stdlib.h>

typedef struct cbufint64_t_ {
    size_t   sz;  /* size of circular buffer                  */
    int64_t* buf; /* array holding the integers in the buffer */
    size_t   nxt; /* next free position                       */
    size_t   n;   /* number of elements currently in buffer   */
} cbufint64_t;

cbufint64_t* cbufint64_new(const size_t sz);
void cbufint64_free(cbufint64_t* cbufint64);
void cbufint64_clear(cbufint64_t* cbufint64);
void cbufint64_push(cbufint64_t* cbufint64, int64_t x);
int64_t cbufint64_top(cbufint64_t* cbufint64);
int64_t cbufint64_get(const cbufint64_t* cbufint64, const size_t pos);

#endif // TSC_CBUFINT64_H

