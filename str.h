/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_STR_H
#define TSC_STR_H

#include <stdint.h>
#include <stdlib.h>

typedef struct str_t_ {
    unsigned char* s;    /* null-terminated string */
    uint32_t       n;    /* length of s */
    uint32_t       size; /* bytes allocated for s */
} str_t;

str_t* str_new(void);
void str_free(str_t* str);
void str_clear(str_t* str);
void str_reserve(str_t* str, const uint32_t sz);
void str_extend(str_t* str, const uint32_t ex);
void str_trunc(str_t* str, const uint32_t tr);
void str_append_str(str_t* str, const str_t* app);
void str_append_cstr(str_t* str , const char* cstr);
void str_append_char(str_t* str, char c);
void str_copy_str(str_t* dest, const str_t* src);
void str_copy_cstr(str_t* dest, const char* src, const size_t n);

#endif /* TSC_STR_H */
