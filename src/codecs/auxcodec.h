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

#ifndef TSC_AUXCODEC_H
#define TSC_AUXCODEC_H

#include "../tvclib/str.h"
#include <stdint.h>
#include <stdio.h>

// Encoder
// -----------------------------------------------------------------------------

typedef struct auxenc_t_ {
    size_t in_sz;   // Accumulated input size
    size_t rec_cnt; // No. of records processed in the current block
    str_t  *tmp;    // Temporal storage for e.g. prediction residues
} auxenc_t;

auxenc_t * auxenc_new(void);
void auxenc_free(auxenc_t *auxenc);
void auxenc_add_record(auxenc_t       *auxenc,
                       const char     *qname,
                       const uint16_t flag,
                       const char     *rname,
                       const uint8_t  mapq,
                       const char     *rnext,
                       const uint32_t pnext,
                       const int64_t  tlen,
                       const char     *opt);
size_t auxenc_write_block(auxenc_t *auxenc, FILE *fp);

// Decoder
// -----------------------------------------------------------------------------

typedef struct auxdec_t_ {
    size_t out_sz; // Accumulated output size
} auxdec_t;

auxdec_t * auxdec_new(void);
void auxdec_free(auxdec_t *auxdec);
size_t auxdec_decode_block(auxdec_t *auxdec,
                           FILE     *fp,
                           str_t    **qname,
                           uint16_t *flag,
                           str_t    **rname,
                           uint8_t  *mapq,
                           str_t    **rnext,
                           uint32_t *pnext,
                           int64_t  *tlen,
                           str_t    **opt);

#endif // TSC_AUXCODEC_H

