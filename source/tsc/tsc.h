#ifndef TSC_TSC_H
#define TSC_TSC_H

#include "str.h"
#include <stdbool.h>
#include <stdio.h>

#define KB 1000LL
#define MB (KB*1000LL)
// #define GB (MB*1000LL)

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
extern unsigned int tsc_blocksz;

#endif // TSC_TSC_H
