/******************************************************************************
 * Copyright (c) 2015 Institut fuer Informationsverarbeitung (TNT)            *
 * Contact: Jan Voges <jvoges@tnt.uni-hannover.de>                            *
 *                                                                            *
 * This file is part of tsc.                                                  *
 ******************************************************************************/

#ifndef TSC_TSCLIB_H
#define TSC_TSCLIB_H

#include "str.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG(c,...) fprintf(stderr, "Debug: %s:%d: %s: "c"\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#define KB 1024LL
#define MB (KB * 1024LL)
#define GB (MB * 1024LL)

#define TSC_BLOCK_SZ_DEFAULT 100000

typedef enum {
    TSC_MODE_COMPRESS,
    TSC_MODE_DECOMPRESS
} tsc_mode_t;

extern str_t* tsc_prog_name;
extern str_t* tsc_version;
extern str_t* tsc_in_fname;
extern str_t* tsc_out_fname;
extern FILE* tsc_in_fp;
extern FILE* tsc_out_fp;
extern size_t tsc_block_sz;
extern tsc_mode_t tsc_mode;
extern bool tsc_verbose;

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

