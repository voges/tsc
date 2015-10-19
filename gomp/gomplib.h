/*
 * Copyright (c) 2015 
 * Leibniz Universitaet Hannover, Institut fuer Informationsverarbeitung (TNT)
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
 * along with gomp. If not, see <http://www.gnu.org/licenses/>
 */

#ifndef GOMP_GOMPLIB_H
#define GOMP_GOMPLIB_H

#include "./str/str.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Safe debug macro */
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
#define MB (KB*1024LL)
#define GB (MB*1024LL)

typedef enum {
    GOMP_MODE_COMPRESS,
    GOMP_MODE_DECOMPRESS,
    GOMP_MODE_INFO
} gomp_mode_t;

typedef enum {
    GOMP_IN_FMT_SAM,
    GOMP_IN_FMT_FQ
} gomp_in_fmt_t;

extern str_t *gomp_prog_name;
extern str_t *gomp_version;
extern str_t *gomp_in_fname;
extern str_t *gomp_out_fname;
extern FILE *gomp_in_fp;
extern FILE *gomp_out_fp;
extern gomp_mode_t gomp_mode;
extern gomp_in_fmt_t gomp_in_fmt;
extern bool gomp_stats;
extern bool gomp_time;
extern bool gomp_verbose;

void gomp_cleanup(void);
void gomp_abort(void);
void gomp_error(const char *fmt, ...);
void gomp_warning(const char *fmt, ...);
void gomp_log(const char *fmt, ...);
void gomp_vlog(const char *fmt, ...); /* verbose logging */
void* gomp_malloc(const size_t n);
void* gomp_realloc(void *ptr, const size_t n);
FILE* gomp_fopen(const char *fname, const char *mode);
void gomp_fclose(FILE *fp);

#endif /* GOMP_GOMPLIB_H */

