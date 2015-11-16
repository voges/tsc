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

typedef struct samrec_t_ {
    char     line[8*MB];

    char     *qname; // Query template NAME
    uint16_t flag;   // bitwise FLAG (uint16_t)
    char     *rname; // Reference sequence NAME
    uint32_t pos;    // 1-based leftmost mapping POSition (uint32_t)
    uint8_t  mapq;   // MAPping Quality (uint8_t)
    char     *cigar; // CIGAR string
    char     *rnext; // Ref. name of the mate/NEXT read
    uint32_t pnext;  // Position of the mate/NEXT read (uint32_t)
    int64_t  tlen;   // observed Template LENgth (int64_t)
    char     *seq;   // segment SEQuence
    char     *qual;  // QUALity scores
    char     *opt;   // OPTional information
} samrec_t;

#endif // TSC_SAMREC_H

