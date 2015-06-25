/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tntlib.                                               *
 ******************************************************************************/

#ifndef TNTLIB_COMMON_H
#define TNTLIB_COMMON_H

#include <stdio.h>
#include <stdlib.h>

/* Safe debug macro. */
#if DBG
    #define DEBUG(c,...)\
        do {\
            fprintf(stderr, "Debug: %s:%d: %s: "c"\n", __FILE__, __LINE__, \
                    __FUNCTION__, ##__VA_ARGS__);\
        } while (false)
#else
    #define DEBUG(c,...) do { } while (false)
#endif

#define KB 1024LL
#define MB (KB * 1024LL)
#define GB (MB * 1024LL)

void tnt_abort(void);
void tnt_error(const char* fmt, ...);
void tnt_warning(const char* fmt, ...);
void tnt_log(const char* fmt, ...);
void* tnt_malloc(const size_t n);
void* tnt_realloc(void* ptr, const size_t n);
FILE* tnt_fopen(const char* fname, const char* mode);
void tnt_fclose(FILE* fp);

#endif /* TNTLIB_COMMON_H */

