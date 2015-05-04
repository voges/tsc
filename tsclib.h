/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_TSCLIB_H
#define TSC_TSCLIB_H

#include <stdio.h>
#include "str.h"

#define DEBUG(c,...) fprintf(stderr, "Debug: %s:%d: %s: "c"\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)

#define KB 1024LL
#define MB KB * 1024LL
#define GB MB * 1024LL

extern str_t* tsc_prog_name;
extern str_t* tsc_in_fname;
extern FILE* tsc_in_fp;
extern str_t* tsc_out_fname;
extern FILE* tsc_out_fp;

typedef enum {
    TSC_MODE_COMPRESS,
    TSC_MODE_DECOMPRESS
} tsc_mode_t;

extern tsc_mode_t tsc_mode;

void tsc_cleanup(void);
void tsc_abort(void);
void tsc_error(const char* fmt, ...);
void tsc_warning(const char* fmt, ...);
void tsc_log(const char* fmt, ...);
void* tsc_malloc_or_die(const size_t n);
void* tsc_realloc_or_die(void* ptr, const size_t n);
FILE* tsc_fopen_or_die(const char* fname, const char* mode);
void tsc_fclose_or_die(FILE* fp);

#endif /* TSC_TSCLIB_H */

