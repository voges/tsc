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

#ifndef TSC_SAMREC_H
#define TSC_SAMREC_H

#include "tsclib.h"
#include <stdint.h>

//
// Structure of a SAM alignment line: The 11 mandatory fields are
// TAB-delimited. Optional information is stored as 12th field.
// Data types have been selected according to the SAM format specification.
//

enum {
    SAMREC_QNAME, // Query template NAME
    SAMREC_FLAG,  // bitwise FLAG (uint16_t)
    SAMREC_RNAME, // Reference sequence NAME
    SAMREC_POS,   // 1-based leftmost mapping POSition (uint32_t)
    SAMREC_MAPQ,  // MAPping Quality (uint8_t)
    SAMREC_CIGAR, // CIGAR string
    SAMREC_RNEXT, // Ref. name of the mate/NEXT read
    SAMREC_PNEXT, // Position of the mate/NEXT read (uint32_t)
    SAMREC_TLEN,  // observed Template LENgth (int64_t)
    SAMREC_SEQ,   // segment SEQuence
    SAMREC_QUAL,  // QUALity scores
    SAMREC_OPT    // OPTional information
};

typedef struct samrec_t_ {
    char     line[8*MB];

    char     *qname;
    uint16_t flag;
    char     *rname;
    uint32_t pos;
    uint8_t  mapq;
    char     *cigar;
    char     *rnext;
    uint32_t pnext;
    int64_t  tlen;
    char     *seq;
    char     *qual;
    char     *opt;
} samrec_t;

#endif // TSC_SAMREC_H

