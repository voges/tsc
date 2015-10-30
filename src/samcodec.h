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

#ifndef TSC_SAMCODEC_H
#define TSC_SAMCODEC_H

#include "samparser.h"
#include "codecs/auxcodec.h"
#include "codecs/nuccodec.h"
#include "codecs/qualcodec.h"
#include "tvclib/str.h"
#include <stdio.h>

#define SAMCODEC_REC_N 10000LL // No. of records per block

typedef struct samenc_t_ {
    FILE        *ifp;
    FILE        *ofp;
    samparser_t *samparser;
    auxenc_t    *auxenc;
    nucenc_t    *nucenc;
    qualenc_t   *qualenc;
} samenc_t;

samenc_t * samenc_new(FILE *ifp, FILE *ofp);
void samenc_free(samenc_t *samenc);
void samenc_encode(samenc_t *samenc);

typedef struct samdec_t_ {
    FILE      *ifp;
    FILE      *ofp;
    auxdec_t  *auxdec;
    nucdec_t  *nucdec;
    qualdec_t *qualdec;
} samdec_t;

samdec_t * samdec_new(FILE *ifp, FILE *ofp);
void samdec_free(samdec_t *samdec);
void samdec_decode(samdec_t *samdec);
void samdec_info(samdec_t *samdec);

#endif // TSC_SAMCODEC_H

