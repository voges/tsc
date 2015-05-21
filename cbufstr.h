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
    unsigned int sz;  /*< size of circular buffer                 */
    str_t** buf;      /*< array holding the strings in the buffer */
    unsigned int pos; /*< current position                        */
} cbufstr_t;

cbufstr_t* cbufstr_new(const unsigned int n);
void cbufstr_free(cbufstr_t* cbufstr);
void cbufstr_add(cbufstr_t* cbufstr, const char* s);
str_t* cbufstr_elem(const cbufstr_t* cbufstr, const unsigned int pos);

#endif // TSC_CBUFSTR_H

