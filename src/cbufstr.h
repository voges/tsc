/*****************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)           *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                           *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_CBUFSTR_H
#define TSC_CBUFSTR_H

#include "str.h"

typedef struct cbufstr_t_ {
    size_t  sz;  /* size of circular buffer                 */
    str_t** buf; /* array holding the strings in the buffer */
    size_t  pos; /* current position                        */
    size_t   n;  /* number of elements currently in buffer */
} cbufstr_t;

cbufstr_t* cbufstr_new(const size_t sz);
void cbufstr_free(cbufstr_t* cbufstr);
void cbufstr_clear(cbufstr_t* cbufstr);
void cbufstr_push(cbufstr_t* cbufstr, const char* s);
str_t* cbufstr_get(const cbufstr_t* cbufstr, const size_t pos);

#endif // TSC_CBUFSTR_H

