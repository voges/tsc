/*
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)
 * Contact: Jan Voges <voges@tnt.uni-hannover.de>
 */

/*
 * This file is part of gomp.
 *
 * Gomp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Gomp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gomp. If not, see <http: *www.gnu.org/licenses/>
 */

/*
 * Encapsulated strings. All functions take care of appending the
 * terminating '\0' byte themselves.
 */

#ifndef GOMP_STR_H
#define GOMP_STR_H

#include <stdint.h>
#include <stdlib.h>

typedef struct str_t_ {
    char*  s;   /* null-terminated string */
    size_t len; /* length of s            */
    size_t sz;  /* bytes allocated for s  */
} str_t;

str_t* str_new(void);
void str_free(str_t* str);
void str_clear(str_t* str);
void str_reserve(str_t* str, const size_t sz);
void str_extend(str_t* str, const size_t ex);
void str_trunc(str_t* str, const size_t tr);
void str_append_str(str_t* str, const str_t* app);
void str_append_cstr(str_t* str , const char* cstr);
void str_append_cstrn(str_t* str, const char* cstr, const size_t len);
void str_append_char(str_t* str, const char c);
void str_copy_str(str_t* dest, const str_t* src);
void str_copy_cstr(str_t* dest, const char* src);

#endif /* GOMP_STR_H */

