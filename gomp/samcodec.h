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

#ifndef GOMP_SAMCODEC_H
#define GOMP_SAMCODEC_H

#include "./sub-block_codecs/auxcodec.h"
#include "./sub-block_codecs/nuccodec.h"
#include "./sub-block_codecs/qualcodec.h"
#include "samparser.h"
#include "./str/str.h"
#include <stdio.h>

#define FILECODEC_BLK_LC 10000LL /* no. of SAM lines per block */

/*
 * Encoder
 */
typedef struct samenc_t_ {
    FILE*        ifp;
    FILE*        ofp;
    samparser_t* samparser;
    auxenc_t*    auxenc;
    nucenc_t*    nucenc;
    qualenc_t*   qualenc;
} samenc_t;

samenc_t* samenc_new(FILE* ifp, FILE* ofp);
void samenc_free(samenc_t* samenc);
void samenc_encode(samenc_t* samenc);

/*
 * Decoder
 */
typedef struct samdec_t_ {
    FILE*      ifp;
    FILE*      ofp;
    auxdec_t*  auxdec;
    nucdec_t*  nucdec;
    qualdec_t* qualdec;
} samdec_t;

samdec_t* samdec_new(FILE* ifp, FILE* ofp);
void samdec_free(samdec_t* samdec);
void samdec_decode(samdec_t* samdec);
void samdec_info(samdec_t* samdec);

#endif /* GOMP_SAMCODEC_H */

