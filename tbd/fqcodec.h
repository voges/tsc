//
// Copyright (c) 2015 
// Leibniz Universitaet Hannover, Institut fuer Informationsverarbeitung (TNT)
// Contact: Jan Voges <voges@tnt.uni-hannover.de>
//

//
// This file is part of gomp.
//
// Gomp is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Gomp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with gomp. If not, see <http://www.gnu.org/licenses/>
//

#ifndef GOMP_FQCODEC_H
#define GOMP_FQCODEC_H

#include "fqparser.h"
#include "./str/str.h"
#include "./sub-block_codecs/fq_rname_codec.h"
#include "./sub-block_codecs/fq_seq_codec.h"
#include "./sub-block_codecs/fq_qual_codec.h"
#include <stdio.h>

// Encoder
typedef struct fqenc_t_ {
    FILE       *ifp;
    FILE       *ofp;
    fqparser_t *fqparser;
    rheadenc_t *rheadenc;
    seqenc_t   *seqenc;
    qualenc_t  *qualenc;
} fqenc_t;

fqenc_t * fqenc_new(FILE *ifp, FILE *ofp);
void fqenc_free(fqenc_t *fqenc);
void fqenc_encode(fqenc_t *fqenc);

// Decoder
typedef struct fqdec_t_ {
    FILE       *ifp;
    FILE       *ofp;
    rheaddec_t *rheaddec;
    seqdec_t   *seqdec;
    qualdec_t  *qualdec;
} fqdec_t;

fqdec_t * fqdec_new(FILE *ifp, FILE *ofp);
void fqdec_free(fqdec_t *fqdec);
void fqdec_decode(fqdec_t *fqdec);
void fqdec_info(fqdec_t *fqdec);

#endif /* GOMP_FQCODEC_H */

