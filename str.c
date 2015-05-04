/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#include "str.h"
#include <string.h>
#include "tsclib.h"

static void str_init(str_t* str)
{
    str->s = NULL;
    str->n = 0;
    str->size = 0;

    /* Init string with terminating NULL byte */
    str_reserve(str, 1);
    str->s[str->n] = '\0';
}

str_t* str_new(void)
{
    str_t* str = (str_t*)tsc_malloc_or_die(sizeof(str_t));
    str_init(str);
    return str;
}

void str_free(str_t* str)
{
    if (str != NULL) {
        if (str->s != NULL) {
            free((void*)str->s);
            str->s = NULL;
        }
        free((void*)str);
        str = NULL;
    }
    else { /* str == NULL */
        tsc_error("Tried to free NULL pointer. Aborting.");
    }
}

void str_clear(str_t* str)
{
    if (str->s != NULL) {
        free((void*)str->s);
        str->s = NULL;
    }
    str_init(str);
}

void str_reserve(str_t* str, const uint32_t sz)
{
    str->size = sz;
    str->s = (unsigned char*)tsc_realloc_or_die(str->s, str->size * sizeof(unsigned char));
}

void str_extend(str_t* str, const uint32_t ex)
{
    str_reserve(str, str->size + ex);
}

void str_trunc(str_t* str, const uint32_t tr)
{
    str->n -= tr;
    str->size -= tr;
    str_reserve(str, str->size);
    str->s[str->n] = '\0';
}

void str_append_str(str_t* str, const str_t* app)
{
    str_extend(str, app->n);
    memcpy(str->s + str->n, app->s, app->n);
    str->n += app->n;
    str->s[str->n] = '\0';
}

void str_append_cstr(str_t* str, const char* cstr)
{
    uint32_t len = (uint32_t)strlen(cstr);
    str_extend(str, len);
    memcpy(str->s + str->n, cstr, len);
    str->n += len;
    str->s[str->n] = '\0';
}

void str_append_char(str_t* str, const char c)
{
    str_extend(str, 1);
    str->s[str->n++] = c;
    str->s[str->n] = '\0';
}

void str_copy_str(str_t* dest, const str_t* src)
{
    str_clear(dest);
    str_reserve(dest, src->n + 1);
    memcpy(dest->s, src->s, src->n);
    dest->n = src->n;
    dest->s[dest->n] = '\0';
}

void str_copy_cstr(str_t* str, const char* cstr)
{
    str_clear(str);
    uint32_t len = (uint32_t)strlen(cstr);
    str_reserve(str, len + 1);
    memcpy(str->s, cstr, len);
    str->n = len;
    str->s[str->n] = '\0';
}

