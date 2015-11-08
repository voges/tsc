//
// Copyright (c) 2015
// Leibniz Universitaet Hannover, Institut fuer Informationsverarbeitung (TNT)
// Contact: Jan Voges <voges@tnt.uni-hannover.de>
//

//
// This file is part of tsc.
//
// Tsc is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Tsc is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with tsc. If not, see <http://www.gnu.org/licenses/>
//

#ifndef TSC_TSCLIB_H
#define TSC_TSCLIB_H

#include "tvclib/str.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Safe debug macro
#if DBG
    #define DEBUG(c,...)\
        do {\
            fprintf(stderr, "Debug: %s:%d: %s: "c"\n", __FILE__, __LINE__, \
                    __FUNCTION__, ##__VA_ARGS__);\
        } while (false)
#else
    #define DEBUG(c,...) do { } while (false)
#endif

// Decimal prefixes
#define KB 1000LL
#define MB (KB*1000LL)
#define GB (MB*1000LL)

typedef enum {
    TSC_MODE_COMPRESS,
    TSC_MODE_DECOMPRESS,
    TSC_MODE_INFO
} tsc_mode_t;

extern str_t *tsc_prog_name;
extern str_t *tsc_version;
extern str_t *tsc_in_fname;
extern str_t *tsc_out_fname;
extern FILE *tsc_in_fp;
extern FILE *tsc_out_fp;
extern tsc_mode_t tsc_mode;
extern bool tsc_stats;
extern bool tsc_verbose;
extern bool tsc_warn;

void tsc_cleanup(void);
void tsc_abort(void);
void tsc_error(const char *fmt, ...);
void tsc_warning(const char *fmt, ...);
void tsc_log(const char *fmt, ...);
void tsc_vlog(const char *fmt, ...);
void * tsc_malloc(const size_t n);
void * tsc_realloc(void *ptr, const size_t n);
FILE * tsc_fopen(const char *fname, const char *mode);
void tsc_fclose(FILE *fp);

#endif // TSC_TSCLIB_H

