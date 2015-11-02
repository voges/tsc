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

#ifndef TSC_QUALCODEC_H
#define TSC_QUALCODEC_H


#include "../tvclib/str.h"
#include <stdio.h>

// Encoder
// -----------------------------------------------------------------------------

typedef struct qualenc_t_ {
    size_t in_sz;   // Accumulated input size
    size_t rec_cnt; // No. of records processed in the current block
    str_t  *tmp;    // Temporal storage for e.g. prediction residues
} qualenc_t;

qualenc_t * qualenc_new(void);
void qualenc_free(qualenc_t *qualenc);
void qualenc_add_record(qualenc_t *qualenc, const char *qual);
size_t qualenc_write_block(qualenc_t *qualenc, FILE *fp);

// Decoder
// -----------------------------------------------------------------------------

typedef struct qualdec_t_ {
    size_t out_sz; // Accumulated output size
} qualdec_t;

qualdec_t * qualdec_new(void);
void qualdec_free(qualdec_t *qualdec);
size_t qualdec_decode_block(qualdec_t *qualdec, FILE *fp, str_t **qual);

#endif // TSC_QUALCODEC_H

