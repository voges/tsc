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

