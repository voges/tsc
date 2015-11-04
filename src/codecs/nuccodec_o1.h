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

#ifndef TSC_NUCCODEC_O1_H
#define TSC_NUCCODEC_O1_H

#include "../tvclib/bbuf.h"
#include "../tvclib/cbufint64.h"
#include "../tvclib/cbufstr.h"
#include "../tvclib/str.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define WINDOW_SZ 10
#define ALPHA 0.5

// Encoder
// -----------------------------------------------------------------------------

typedef struct nucenc_t_ {
    size_t in_sz;     // Accumulated input size
    size_t rec_cnt;   // No. of records processed in the current block
    size_t trec_cnt;  // Total # of records processed
    bool first;       // 'false', if first line has not been processed yet
    size_t skip_cnt;  // # of skipped records (POS, CIGAR, or SEQ missing) in the
                      // current block
    size_t tskip_cnt; // Total # of skipped records
    size_t poff_cnt;  // # of I-Frames added to due large position offset
    size_t tpoff_cnt; // Total # of added I-Frames
    double stogy_mu;  // Mean STOGY length
    double snp_mu;    // Mean SNP length
    double trail_mu;  // Mean TRAIL length

    str_t  *stat_fname; // File name for statistics file
    FILE   *stat_fp;    // File pointer to statistics file

    str_t  *tmp;      // Temporal storage for e.g. prediction residues

    // Circular buffers
    cbufint64_t *neo_cbuf;
    cbufint64_t *pos_cbuf;
    cbufstr_t   *exs_cbuf;
} nucenc_t;

nucenc_t * nucenc_new(void);
void nucenc_free(nucenc_t *nucenc);
void nucenc_add_record(nucenc_t       *nucenc,
                       const uint32_t pos,
                       const char     *cigar,
                       const char     *seq);
size_t nucenc_write_block(nucenc_t *nucenc, FILE *fp);

// Decoder
// -----------------------------------------------------------------------------

typedef struct nucdec_t_ {
    size_t out_sz; // Accumulated output size
} nucdec_t;

nucdec_t * nucdec_new(void);
void nucdec_free(nucdec_t *nucdec);
size_t nucdec_decode_block(nucdec_t *nucdec,
                           FILE     *fp,
                           uint32_t *pos,
                           str_t    **cigar,
                           str_t    **seq);

#endif // TSC_NUCCODEC_O1_H

