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

#ifndef TSC_PAIRCODEC_H
#define TSC_PAIRCODEC_H

#include "../tvclib/str.h"
#include <stdint.h>
#include <stdio.h>

// Encoder
// -----------------------------------------------------------------------------

typedef struct pairenc_t_ {
    size_t in_sz;   // Accumulated input size
    size_t rec_cnt; // No. of records processed in the current block
    str_t  *tmp;    // Temporal storage for e.g. prediction residues
} pairenc_t;

pairenc_t * pairenc_new(void);
void pairenc_free(pairenc_t *pairenc);
void pairenc_add_record(pairenc_t      *pairenc,
                        const char     *rnext,
                        const uint32_t pnext,
                        const int64_t  tlen);
size_t pairenc_write_block(pairenc_t *pairenc, FILE *fp);

// Decoder
// -----------------------------------------------------------------------------

typedef struct pairdec_t_ {
    size_t out_sz; // Accumulated output size
} pairdec_t;

pairdec_t * pairdec_new(void);
void pairdec_free(pairdec_t *pairdec);
size_t pairdec_decode_block(pairdec_t *pairdec,
                            FILE      *fp,
                            str_t     **rnext,
                            uint32_t  *pnext,
                            int64_t   *tlen);

#endif // TSC_PAIRCODEC_H

