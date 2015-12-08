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

#ifndef TSC_IDCODEC_H
#define TSC_IDCODEC_H


#include "../tvclib/str.h"
#include <stdio.h>

// Encoder
// -----------------------------------------------------------------------------

typedef struct idenc_t_ {
    size_t in_sz;   // Accumulated input size
    size_t rec_cnt; // No. of records processed in the current block
    str_t  *tmp;    // Temporal storage for e.g. prediction residues
} idenc_t;

idenc_t * idenc_new(void);
void idenc_free(idenc_t *idenc);
void idenc_add_record(idenc_t *idenc, const char *qname);
size_t idenc_write_block(idenc_t *idenc, FILE *fp);

// Decoder
// -----------------------------------------------------------------------------

typedef struct iddec_t_ {
    size_t out_sz; // Accumulated output size
} iddec_t;

iddec_t * iddec_new(void);
void iddec_free(iddec_t *iddec);
size_t iddec_decode_block(iddec_t *iddec, FILE *fp, str_t **qname);

#endif // TSC_IDCODEC_H

