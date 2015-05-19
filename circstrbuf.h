/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_CIRCSTRBUF_H
#define TSC_CIRCSTRBUF_H

#include "str.h"

typedef struct circstrbuf_t_ {
    unsigned int n;
    str_t** buf;
    unsigned int pos;
} circstrbuf_t;

circstrbuf_t* circstrbuf_new(unsigned int n);
void circstrbuf_free(circstrbuf_t* circstrbuf);
void circstrbuf_add(circstrbuf_t* circstrbuf, const char* s);
str_t* circstrbuf_elem(circstrbuf_t* circstrbuf, unsigned int pos);

#endif // TSC_CIRCSTRBUF_H

