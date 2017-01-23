/*
 * The copyright in this software is being made available under the TNT
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2015, Leibniz Universitaet Hannover, Institut fuer
 * Informationsverarbeitung (TNT)
 * Contact: <voges@tnt.uni-hannover.de>
 * All rights reserved.
 *
 * * Redistribution in source or binary form is not permitted.
 *
 * * Use in source or binary form is only permitted in the context of scientific
 *   research.
 *
 * * Commercial use without specific prior written permission is prohibited.
 *   Neither the name of the TNT nor the names of its contributors may be used
 *   to endorse or promote products derived from this software without specific
 *   prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

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
#define ALPHA 0.9

// Encoder
// -----------------------------------------------------------------------------

typedef struct nucenc_t_ {
    size_t in_sz;       // Accumulated input size
    size_t rec_blk_cnt; // No. of records processed in the current block
    size_t rec_tot_cnt; // Total # of records processed
    bool   first;       // 'false', if first line has not been processed yet
    str_t  *tmp;        // Temporal storage for e.g. prediction residues
    str_t  *rname_prev; // Holding current RNAME

    // Statistics
    str_t  *stat_fname; // File name for statistics file
    FILE   *stat_fp;    // File pointer to statistics file
    size_t m_blk_cnt;
    size_t m_tot_cnt;
    size_t i_blk_cnt;
    size_t i_tot_cnt;
    double stogy_mu;  // Mean STOGY length
    double mod_mu;    // Mean MOD length
    double trail_mu;  // Mean TRAIL length

    // Circular buffers
    cbufint64_t *neo_cbuf;
    cbufint64_t *pos_cbuf;
    cbufstr_t   *exs_cbuf;
} nucenc_t;

nucenc_t * nucenc_new(void);
void nucenc_free(nucenc_t *nucenc);
void nucenc_add_record(nucenc_t       *nucenc,
                       const char     *rname,
                       const uint32_t pos,
                       const char     *cigar,
                       const char     *seq);
size_t nucenc_write_block(nucenc_t *nucenc, FILE *fp);

// Decoder
// -----------------------------------------------------------------------------

typedef struct nucdec_t_ {
    size_t out_sz; // Accumulated output size

    // Circular buffers
    cbufint64_t *neo_cbuf;
    cbufint64_t *pos_cbuf;
    cbufstr_t   *exs_cbuf;
} nucdec_t;

nucdec_t * nucdec_new(void);
void nucdec_free(nucdec_t *nucdec);
size_t nucdec_decode_block(nucdec_t *nucdec,
                           FILE     *fp,
                           str_t    **rname,
                           uint32_t *pos,
                           str_t    **cigar,
                           str_t    **seq);

#endif // TSC_NUCCODEC_O1_H

