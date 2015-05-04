/*****************************************************************************
 * Copyright (c) 2015 Jan Voges <jvoges@tnt.uni-hannover.de>                 *
 *                                                                           *
 * This file is part of tsc.                                                 *
 *****************************************************************************/

#ifndef TSC_SAMRECORD_H
#define TSC_SAMRECORD_H

#include <stdint.h>
#include "str.h"
#include "tsclib.h"

/*
 * Structure of a SAM alignment line: The 11 mandatory fields are
 * TAB-delimited. Optional information is stored as 12th field.
 * Data types have been selected according to the SAM format specification.
 */

enum {
    QNAME, /* query template name */
    RNAME, /* reference sequence name */
    CIGAR, /* CIGAR string */
    RNEXT, /* ref. name of the mate/next read */
    SEQ,   /* segment sequence */
    QUAL,  /* ASCII of Phred-scaled base quality + 33 */
    OPT    /* optional information */
};

enum {
    FLAG,  /* bitwise flag (uint16_t) */
    POS,   /* 1-based leftmost mapping position (uint32_t) */
    MAPQ,  /* mapping quality (uint8_t) */
    PNEXT, /* position of the mate/next read (uint32_t) */
    TLEN   /* observed template length (int64_t) */
};

typedef struct samrecord_t_ {
    char line[8 * MB];
    char* str_fields[7];
    int64_t int_fields[5];
} samrecord_t;

samrecord_t* samrecord_new(void);
void samrecord_free(samrecord_t* samrecord);
void samrecord_clear(samrecord_t* samrecord);

#endif /* TSC_SAMRECORD_H */
