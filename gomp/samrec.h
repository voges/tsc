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

#ifndef GOMP_SAMREC_H
#define GOMP_SAMREC_H

#include "gomplib.h"
#include <stdint.h>

/*
 * Structure of a SAM alignment line: The 11 mandatory fields are
 * TAB-delimited. Optional information is stored as 12th field.
 * Data types have been selected according to the SAM format specification [1].
 *
 * [1] The SAM/BAM Format Specification Working Group: Sequence Alignment/Map
 *     Format Specification, 28 December 2014, Available:
 *     https://github.com/samtools/hts-specs
 */

enum {
    QNAME, /* Query template NAME             */
    RNAME, /* Reference sequence NAME         */
    CIGAR, /* CIGAR string                    */
    RNEXT, /* Ref. name of the mate/NEXT read */
    SEQ,   /* segment SEQuence                */
    QUAL,  /* QUALity scores                  */
    OPT    /* OPTional information            */
};

enum {
    FLAG,  /* bitwise FLAG (uint16_t)                      */
    POS,   /* 1-based leftmost mapping POSition (uint32_t) */
    MAPQ,  /* MAPping Quality (uint8_t)                    */
    PNEXT, /* Position of the mate/NEXT read (uint32_t)    */
    TLEN   /* observed Template LENgth (int64_t)           */
};

typedef struct samrec_t_ {
    char    line[8*MB];
    char*   str_fields[7];
    int64_t int_fields[5];
} samrec_t;

#endif /* GOMP_SAMREC_H */

